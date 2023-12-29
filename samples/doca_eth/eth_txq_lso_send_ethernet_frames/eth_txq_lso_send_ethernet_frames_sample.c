/*
 * Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <endian.h>

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_ctx.h>
#include <doca_eth_txq.h>
#include <doca_eth_txq_cpu_data_path.h>
#include <doca_error.h>
#include <doca_log.h>

#include "common.h"
#include "eth_common.h"

DOCA_LOG_REGISTER(ETH_TXQ_LSO_SEND_ETHERNET_FRAMES);

#define SLEEP_IN_NANOS (10 * 1000)				/* sample the task every 10 microseconds  */
#define MAX_BURST_SIZE 256					/* Max burst size to set for eth_txq */
#define MAX_LSO_HEADER_SIZE 64					/* Max header size in LRO packet to set for eth_txq */
#define MAX_LIST_LNEGTH 1					/* Max number of elements in a doca_buf */
#define MSS 1500						/* Max Segment Size in LSO tasks to set for eth_txq */
#define BUFS_NUM 1						/* Number of DOCA buffers */
#define LOG_TASKS_NUM 1						/* log of tasks number */
#define LSO_PKT_SIZE 4000					/* Size of the packet in doca_eth_txq_task_lso_send task */
#define LSO_SEND_TASK_USER_DATA 0x56789				/* User data for LSO send task */
#define ETHER_TYPE_IPV4 0x0800					/* IPV4 type */

struct eth_txq_lso_sample_objects {
	struct eth_core_resources core_resources;		/* A struct to hold ETH core resources */
	struct doca_eth_txq *eth_txq;				/* DOCA ETH TXQ context */
	struct doca_buf *lso_eth_payload_buf;			/* DOCA buffer to contain LSO ethernet payload */
	struct doca_gather_list *lso_pkt_headers;		/* DOCA gaher list to contain LSO packet headers */
	struct doca_eth_txq_task_lso_send *lso_send_task;	/* LSO send task */
	uint8_t *lso_pkt_headers_buf;				/* Buffer to create lso_pkt_headers with */
	uint8_t src_mac_addr[DOCA_DEVINFO_MAC_ADDR_SIZE];	/* Device MAC address */
	uint32_t inflight_tasks;				/* In flight tasks */
};

/*
 * ETH TXQ LSO send task common callback
 *
 * @task_lso_send [in]: Completed task
 * @task_user_data [in]: User provided data, used for identifying the task
 * @ctx_user_data [in]: User provided data, used to store sample state
 */
static void
task_lso_send_common_cb(struct doca_eth_txq_task_lso_send *task_lso_send, union doca_data task_user_data,
			 union doca_data ctx_user_data)
{
	doca_error_t status, status_task;
	struct doca_buf *pkt_payload;
	size_t payload_size;
	uint32_t *inflight_tasks;

	inflight_tasks = ctx_user_data.ptr;
	(*inflight_tasks)--;
	DOCA_LOG_INFO("LSO send task user data is 0x%lx", task_user_data.u64);

	status = doca_eth_txq_task_lso_send_get_pkt_payload(task_lso_send, &pkt_payload);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get packet payload of an LSO send task, err: %s",
			doca_error_get_name(status));
		doca_task_free(doca_eth_txq_task_lso_send_as_doca_task(task_lso_send));
		return;
	}

	status_task = doca_task_get_status(doca_eth_txq_task_lso_send_as_doca_task(task_lso_send));

	status = doca_buf_get_data_len(pkt_payload, &payload_size);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get LSO send packet payload size, err: %s", doca_error_get_name(status));
	} else {
		if (status_task == DOCA_SUCCESS)
			DOCA_LOG_INFO("Sent an LSO packet of payload size %lu succesfully", payload_size);
		else
			DOCA_LOG_ERR("Failed to send an LSO packet of payload size %lu, err: %s", payload_size,
				     doca_error_get_name(status_task));
	}

	status = doca_buf_dec_refcount(pkt_payload, NULL);
	if (status != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to free packet payload buf, err: %s", doca_error_get_name(status));

	doca_task_free(doca_eth_txq_task_lso_send_as_doca_task(task_lso_send));
}

/*
 * Destroy ETH TXQ context related resources
 *
 * @state [in]: eth_txq_lso_sample_objects struct to destroy its ETH TXQ context
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
static doca_error_t
destroy_eth_txq_ctx(struct eth_txq_lso_sample_objects *state)
{
	doca_error_t status;
	enum doca_ctx_states ctx_state;
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_IN_NANOS,
	};

	status = doca_ctx_stop(state->core_resources.core_objs.ctx);
	if (status == DOCA_ERROR_IN_PROGRESS) {
		while (state->inflight_tasks != 0) {
			(void)doca_pe_progress(state->core_resources.core_objs.pe);
			nanosleep(&ts, &ts);
		}

		status = doca_ctx_get_state(state->core_resources.core_objs.ctx, &ctx_state);
		if (status != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed get status of context, err: %s", doca_error_get_name(status));
			return status;
		}

		status = ctx_state == DOCA_CTX_STATE_IDLE ? DOCA_SUCCESS : DOCA_ERROR_BAD_STATE;
	}

	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to stop DOCA context, err: %s", doca_error_get_name(status));
		return status;
	}

	status = doca_eth_txq_destroy(state->eth_txq);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to destroy DOCA ETH TXQ context, err: %s", doca_error_get_name(status));
		return status;
	}

	return DOCA_SUCCESS;
}

/*
 * Destroy DOCA buffers for the packets
 *
 * @state [in]: eth_txq_lso_sample_objects struct to destroy its packet DOCA buffers
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
static doca_error_t
destroy_eth_txq_packet_buffers(struct eth_txq_lso_sample_objects *state)
{
	doca_error_t status;

	status = doca_buf_dec_refcount(state->lso_eth_payload_buf, NULL);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to destroy lso_eth_payload_buf buffer, err: %s", doca_error_get_name(status));
		return status;
	}

	return DOCA_SUCCESS;
}

/*
 * Destroy ETH TXQ tasks
 *
 * @state [in]: eth_txq_lso_sample_objects struct to destroy its tasks
 */
static void
destroy_eth_txq_tasks(struct eth_txq_lso_sample_objects *state)
{
	doca_task_free(doca_eth_txq_task_lso_send_as_doca_task(state->lso_send_task));
}

/*
 * Retrieve ETH TXQ tasks
 *
 * @state [in]: eth_txq_lso_sample_objects struct to retrieve its tasks
 */
static void
retrieve_eth_txq_tasks(struct eth_txq_lso_sample_objects *state)
{
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_IN_NANOS,
	};

	while (state->inflight_tasks != 0) {
		(void)doca_pe_progress(state->core_resources.core_objs.pe);
		nanosleep(&ts, &ts);
	}
}

/*
 * Submit ETH TXQ tasks
 *
 * @state [in]: eth_txq_lso_sample_objects struct to submit its tasks
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
static doca_error_t
submit_eth_txq_tasks(struct eth_txq_lso_sample_objects *state)
{
	doca_error_t status;

	status = doca_task_submit(doca_eth_txq_task_lso_send_as_doca_task(state->lso_send_task));
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit LSO send task, err: %s", doca_error_get_name(status));
		return status;
	}

	state->inflight_tasks++;

	return DOCA_SUCCESS;
}

/*
 * Create ETH TXQ tasks
 *
 * @state [in]: eth_txq_lso_sample_objects struct to create tasks with its ETH TXQ context
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
static doca_error_t
create_eth_txq_tasks(struct eth_txq_lso_sample_objects *state)
{
	doca_error_t status;
	union doca_data user_data;

	user_data.u64 = LSO_SEND_TASK_USER_DATA;
	status = doca_eth_txq_task_lso_send_allocate_init(state->eth_txq, state->lso_eth_payload_buf,
							  state->lso_pkt_headers, user_data,
							  &(state->lso_send_task));
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate LSO send task, err: %s", doca_error_get_name(status));
		return status;
	}

	return DOCA_SUCCESS;
}

/*
 * Create DOCA buffers for the packets
 *
 * @state [in]: eth_txq_lso_sample_objects struct to create its packet DOCA buffers
 * @dest_mac_addr [in]: Destination MAC address to set in ethernet header
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
static doca_error_t
create_eth_txq_lso_packet_buffers(struct eth_txq_lso_sample_objects *state, uint8_t *dest_mac_addr)
{
	doca_error_t status;
	struct ether_hdr *eth_hdr;
	uint8_t *payload;

	status = doca_buf_inventory_buf_get_by_data(state->core_resources.core_objs.buf_inv, state->core_resources.core_objs.src_mmap,
						    state->core_resources.mmap_addr, LSO_PKT_SIZE, &(state->lso_eth_payload_buf));
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create DOCA bufer for LSO ethernet payload, err: %s",
			doca_error_get_name(status));
		return status;
	}

	state->lso_pkt_headers->addr = state->lso_pkt_headers_buf;
	state->lso_pkt_headers->len = sizeof(struct ether_hdr);
	state->lso_pkt_headers->next = NULL;

	/* Create LSO packet header + payload */
	eth_hdr = (struct ether_hdr *)state->lso_pkt_headers_buf;
	payload = state->core_resources.mmap_addr;
	memcpy(&(eth_hdr->src_addr), state->src_mac_addr, DOCA_DEVINFO_MAC_ADDR_SIZE);
	memcpy(&(eth_hdr->dst_addr), dest_mac_addr, DOCA_DEVINFO_MAC_ADDR_SIZE);
	eth_hdr->ether_type = htobe16(ETHER_TYPE_IPV4);
	memset(payload, 0x22, LSO_PKT_SIZE);

	return DOCA_SUCCESS;
}

/*
 * Create ETH TXQ context related resources
 *
 * @state [in]: eth_txq_lso_sample_objects struct to create its ETH TXQ context
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
static doca_error_t
create_eth_txq_ctx(struct eth_txq_lso_sample_objects *state)
{
	doca_error_t status, clean_status;
	union doca_data user_data;

	status = doca_eth_txq_create(state->core_resources.core_objs.dev, MAX_BURST_SIZE, &(state->eth_txq));
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ETH TXQ context, err: %s", doca_error_get_name(status));
		return status;
	}

	status = doca_eth_txq_set_type(state->eth_txq, DOCA_ETH_TXQ_TYPE_REGULAR);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set type, err: %s", doca_error_get_name(status));
		goto destroy_eth_txq;
	}

	status = doca_eth_txq_task_lso_send_set_conf(state->eth_txq, task_lso_send_common_cb,
						     task_lso_send_common_cb, MSS, MAX_LSO_HEADER_SIZE,
						     LOG_TASKS_NUM);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to configure task_lso_send, err: %s", doca_error_get_name(status));
		goto destroy_eth_txq;
	}

	state->core_resources.core_objs.ctx = doca_eth_txq_as_doca_ctx(state->eth_txq);
	if (state->core_resources.core_objs.ctx == NULL) {
		DOCA_LOG_ERR("Failed to retrieve DOCA ETH TXQ context as DOCA context, err: %s",
			doca_error_get_name(status));
		goto destroy_eth_txq;
	}

	status = doca_pe_connect_ctx(state->core_resources.core_objs.pe, state->core_resources.core_objs.ctx);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to connect PE, err: %s", doca_error_get_name(status));
		goto destroy_eth_txq;
	}

	user_data.ptr = &(state->inflight_tasks);
	status = doca_ctx_set_user_data(state->core_resources.core_objs.ctx, user_data);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set user data for DOCA context, err: %s", doca_error_get_name(status));
		goto destroy_eth_txq;
	}

	status = doca_ctx_start(state->core_resources.core_objs.ctx);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to start DOCA context, err: %s", doca_error_get_name(status));
		goto destroy_eth_txq;
	}

	return DOCA_SUCCESS;
destroy_eth_txq:
	clean_status = doca_eth_txq_destroy(state->eth_txq);
	state->eth_txq = NULL;

	if (clean_status != DOCA_SUCCESS)
		return clean_status;

	return status;
}

/*
 * Clean sample resources
 *
 * @state [in]: eth_txq_lso_sample_objects struct to clean
 */
static void
eth_txq_lso_cleanup(struct eth_txq_lso_sample_objects *state)
{
	doca_error_t status;

	if (state->lso_pkt_headers != NULL)
		free(state->lso_pkt_headers);

	if (state->lso_pkt_headers_buf != NULL)
		free(state->lso_pkt_headers_buf);

	if (state->eth_txq != NULL) {
		status = destroy_eth_txq_ctx(state);
		if (status != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to destroy eth_txq_ctx, err: %s", doca_error_get_name(status));
			return;
		}
	}

	if (state->core_resources.core_objs.dev != NULL) {
		status = destroy_eth_core_resources(&(state->core_resources));
		if (status != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to destroy core_resources, err: %s", doca_error_get_name(status));
			return;
		}
	}
}

/*
 * Check if device supports needed capabilities
 *
 * @devinfo [in]: Device info for device to check
 * @return: DOCA_SUCCESS in case the device supports needed capabilities and DOCA_ERROR otherwise
 */
static doca_error_t
check_device(struct doca_devinfo *devinfo)
{
	doca_error_t status;
	uint32_t max_supported_burst_size;
	uint16_t max_supported_lso_header_size;
	uint8_t type_supported;

	status = doca_eth_txq_get_max_burst_size_supported(devinfo, MAX_LIST_LNEGTH, MAX_LSO_HEADER_SIZE,
							   &max_supported_burst_size);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get supported max burst size, err: %s", doca_error_get_name(status));
		return status;
	}

	if (max_supported_burst_size < MAX_BURST_SIZE)
		return DOCA_ERROR_NOT_SUPPORTED;

	status = doca_eth_txq_get_max_lso_header_size_supported(devinfo, &max_supported_lso_header_size);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get supported max lso header size, err: %s", doca_error_get_name(status));
		return status;
	}

	if (max_supported_lso_header_size < MAX_LSO_HEADER_SIZE)
		return DOCA_ERROR_NOT_SUPPORTED;


	status = doca_eth_txq_get_type_supported(devinfo, DOCA_ETH_TXQ_TYPE_REGULAR,
						 DOCA_ETH_TXQ_DATA_PATH_TYPE_CPU, &type_supported);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to check supported type, err: %s", doca_error_get_name(status));
		return status;
	}

	if (type_supported == 0)
		return DOCA_ERROR_NOT_SUPPORTED;

	return DOCA_SUCCESS;
}

/*
 * Run ETH TXQ send ethernet frames
 *
 * @ib_dev_name [in]: IB device name of a doca device
 * @dest_mac_addr [in]: destination MAC address to associate with the ethernet frames
 * @return: DOCA_SUCCESS on success, DOCA_ERROR otherwise
 */
doca_error_t
eth_txq_lso_send_ethernet_frames(const char *ib_dev_name, uint8_t *dest_mac_addr)
{
	doca_error_t status, clean_status;
	struct eth_txq_lso_sample_objects state;
	struct eth_core_config cfg = {
		.mmap_size = LSO_PKT_SIZE * BUFS_NUM,
		.inventory_num_elements = BUFS_NUM,
		.check_device = check_device,
		.ibdev_name = ib_dev_name
	};

	memset(&state, 0, sizeof(struct eth_txq_lso_sample_objects));
	status = allocate_eth_core_resources(&cfg, &(state.core_resources));
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed allocate core resources, err: %s", doca_error_get_name(status));
		return status;
	}

	status = doca_devinfo_get_mac_addr(doca_dev_as_devinfo(state.core_resources.core_objs.dev), state.src_mac_addr,
					   DOCA_DEVINFO_MAC_ADDR_SIZE);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get device MAC address, err: %s", doca_error_get_name(status));
		goto txq_lso_cleanup;
	}

	status = create_eth_txq_ctx(&state);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create/start ETH TXQ context, err: %s", doca_error_get_name(status));
		goto txq_lso_cleanup;
	}

	state.lso_pkt_headers_buf = (uint8_t *)malloc(sizeof(struct ether_hdr));
	if (state.lso_pkt_headers_buf == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory for LSO packet headers");
		status = DOCA_ERROR_NO_MEMORY;
		goto txq_lso_cleanup;
	}

	state.lso_pkt_headers = (struct doca_gather_list *)malloc(sizeof(struct doca_gather_list));
	if (state.lso_pkt_headers == NULL) {
		DOCA_LOG_ERR("Failed to create DOCA gather list node for LSO packet headers");
		status = DOCA_ERROR_NO_MEMORY;
		goto txq_lso_cleanup;
	}

	status = create_eth_txq_lso_packet_buffers(&state, dest_mac_addr);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create packet buffers, err: %s", doca_error_get_name(status));
		goto txq_lso_cleanup;
	}

	status = create_eth_txq_tasks(&state);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create tasks, err: %s", doca_error_get_name(status));
		goto destroy_packet_buffers;
	}

	status = submit_eth_txq_tasks(&state);
	if (status != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit tasks, err: %s", doca_error_get_name(status));
		goto destroy_txq_tasks;
	}

	retrieve_eth_txq_tasks(&state);

	goto txq_lso_cleanup;

destroy_txq_tasks:
	destroy_eth_txq_tasks(&state);
destroy_packet_buffers:
	clean_status = destroy_eth_txq_packet_buffers(&state);
	if (clean_status != DOCA_SUCCESS)
		return clean_status;
txq_lso_cleanup:
	eth_txq_lso_cleanup(&state);

	return status;
}
