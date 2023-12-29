/*
 * Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#include <infiniband/mlx5_api.h>

#include <assert.h>
#include <bsd/string.h>

#include <doca_argp.h>
#include <doca_log.h>

#include "utils.h"

#include "l2_reflector_core.h"
#include "../common/l2_reflector_common.h"

DOCA_LOG_REGISTER(L2_REFLECTOR::CORE);

/* FlexIO application, generated by DPACC stub */
extern struct flexio_app *l2_reflector_device;

#define MATCH_SIZE 64			/* DR Matcher size */
#define LOG2VALUE(l) (1UL << (l))	/* 2^l */
extern flexio_func_t l2_reflector_device_event_handler;
/*
 * Opens the device and returns the device context
 *
 * @device_name [in]: device name
 * @return: device context on success and NULL otherwise
 */
static struct ibv_context *
l2_reflector_open_ibv_device(const char *device_name)
{
	struct ibv_device **dev_list;
	struct ibv_device *dev;
	struct ibv_context *ibv_ctx;
	int dev_idx;

	dev_list = ibv_get_device_list(NULL);
	if (dev_list == NULL) {
		DOCA_LOG_ERR("Failed to get device list");
		return NULL;
	}

	for (dev_idx = 0; dev_list[dev_idx] != NULL; ++dev_idx)
		if (!strcmp(ibv_get_device_name(dev_list[dev_idx]), device_name))
			break;
	dev = dev_list[dev_idx];
	if (dev == NULL) {
		ibv_free_device_list(dev_list);
		DOCA_LOG_ERR("Device %s was not found", device_name);
		return NULL;
	}

	ibv_ctx = ibv_open_device(dev);
	if (ibv_ctx == NULL) {
		ibv_free_device_list(dev_list);
		DOCA_LOG_ERR("Failed to get device context [%s]", device_name);
		return NULL;
	}
	ibv_free_device_list(dev_list);
	return ibv_ctx;
}

doca_error_t
l2_reflector_setup_ibv_device(struct l2_reflector_config *app_cfg)
{
	app_cfg->ibv_ctx = l2_reflector_open_ibv_device(app_cfg->device_name);
	if (app_cfg->ibv_ctx == NULL)
		return DOCA_ERROR_INITIALIZATION;
	app_cfg->pd = ibv_alloc_pd(app_cfg->ibv_ctx);
	if (app_cfg->pd == NULL) {
		DOCA_LOG_ERR("Failed to allocate PD");
		ibv_close_device(app_cfg->ibv_ctx);
		return DOCA_ERROR_DRIVER;
	}
	return DOCA_SUCCESS;
}

doca_error_t
l2_reflector_setup_device(struct l2_reflector_config *app_cfg)
{
	flexio_status result;
	struct flexio_event_handler_attr event_handler_attr = {0};

	/* Create FlexIO Process and mkey */
	result = flexio_process_create(app_cfg->ibv_ctx, l2_reflector_device, NULL, &app_cfg->flexio_process);
	if (result != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Could not create FlexIO process (%d)", result);
		return DOCA_ERROR_DRIVER;
	}

	app_cfg->flexio_uar = flexio_process_get_uar(app_cfg->flexio_process);

	event_handler_attr.host_stub_func = l2_reflector_device_event_handler;
	event_handler_attr.affinity.type = FLEXIO_AFFINITY_STRICT;
	event_handler_attr.affinity.id = 0;
	result = flexio_event_handler_create(app_cfg->flexio_process, &event_handler_attr, &app_cfg->event_handler);
	if (result != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Could not create event handler (%d)", result);
		return DOCA_ERROR_DRIVER;
	}
	return DOCA_SUCCESS;
}

/*
 * Allocates Doorbell record and return its address on the device's memory
 *
 * @process [in]: FlexIO process
 * @dbr_daddr [out]: doorbell record address on the device's memory
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_dbr(struct flexio_process *process, flexio_uintptr_t *dbr_daddr)
{
	__be32 dbr[2] = { 0, 0 };

	if (flexio_copy_from_host(process, dbr, sizeof(dbr), dbr_daddr) != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to copy DBR to device memory");
		return DOCA_ERROR_DRIVER;
	}
	return DOCA_SUCCESS;
}

/*
 * Allocate memory resource for CQ
 *
 * @process [in]: FlexIO process
 * @log_depth [in]: log2 of the CQ depth
 * @app_cq [out]: CQ resource
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_cq_memory(struct flexio_process *process, int log_depth, struct app_transfer_cq *app_cq)
{
	struct mlx5_cqe64 *cq_ring_src, *cqe;
	size_t ring_bsize;
	int i, num_of_cqes;
	const int log_cqe_bsize = 6; /* CQE size is 64 bytes */
	doca_error_t result = DOCA_SUCCESS;
	flexio_status ret;

	/* Allocate DB record */
	result = allocate_dbr(process, &app_cq->cq_dbr_daddr);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate CQ DB record");
		return result;
	}

	num_of_cqes = LOG2VALUE(log_depth);
	ring_bsize = num_of_cqes * LOG2VALUE(log_cqe_bsize);

	cq_ring_src = calloc(num_of_cqes, LOG2VALUE(log_cqe_bsize));

	if (cq_ring_src == NULL) {
		DOCA_LOG_ERR("Failed to allocate CQ ring");
		return DOCA_ERROR_NO_MEMORY;
	}

	cqe = cq_ring_src;
	for (i = 0; i < num_of_cqes; i++)
		mlx5dv_set_cqe_owner(cqe++, 1);

	/* Copy CQEs from host to FlexIO CQ ring */
	ret = flexio_copy_from_host(process, cq_ring_src, ring_bsize, &app_cq->cq_ring_daddr);
	free(cq_ring_src);
	if (ret) {
		DOCA_LOG_ERR("Failed to allocate CQ ring");
		return DOCA_ERROR_DRIVER;
	}

	return DOCA_SUCCESS;
}

/*
 * Allocate memory resource for SQ:
 * -Allocate memory for SQ ring and WQEs
 * -Allocate memory for SQ doorbell record
 *
 * @process [in]: FlexIO process
 * @log_depth [in]: log2 of the SQ depth (number of WQEs)
 * @log_data_bsize [in]: log2 of the SQ data buffer size
 * @sq_transf [out]: SQ resource
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_sq_memory(struct flexio_process *process, int log_depth, int log_data_bsize,
		      struct app_transfer_wq *sq_transf)
{
	const int log_wqe_bsize = 6; /* WQE size is 64 bytes */
	doca_error_t result;

	if (flexio_buf_dev_alloc(process, LOG2VALUE(log_data_bsize), &sq_transf->wqd_daddr) != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate SQ data buffer");
		return DOCA_ERROR_DRIVER;
	}

	if (flexio_buf_dev_alloc(process, LOG2VALUE(log_depth + log_wqe_bsize), &sq_transf->wq_ring_daddr) != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate SQ ring");
		return DOCA_ERROR_DRIVER;
	}

	result = allocate_dbr(process, &sq_transf->wq_dbr_daddr);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to allocate SQ DB record");
		return result;
	}

	return DOCA_SUCCESS;
}

/*
 * Creates mkey for the given memory region
 *
 * @process [in]: FlexIO process
 * @pd [in]: protection domain
 * @daddr [in]: device address of the memory region
 * @log_bsize [in]: log2 of the memory region size
 * @access [in]: access flags
 * @mkey [out]: mkey
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_dpa_mkey(struct flexio_process *process, struct ibv_pd *pd,
			flexio_uintptr_t daddr, int log_bsize, int access, struct flexio_mkey **mkey)
{
	struct flexio_mkey_attr mkey_attr = {0};

	mkey_attr.pd = pd;
	mkey_attr.daddr = daddr;
	mkey_attr.len = LOG2VALUE(log_bsize);
	mkey_attr.access = access;
	if (flexio_device_mkey_create(process, &mkey_attr, mkey) != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to create MKey");
		return DOCA_ERROR_DRIVER;
	}

	return DOCA_SUCCESS;
}

/*
 * Allocate SQ and CQ for the device
 *
 * @app_cfg [in]: application configuration
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_sq(struct l2_reflector_config *app_cfg)
{
	doca_error_t result;
	flexio_status ret;
	uint32_t cq_num;	/* CQ number */
	uint32_t log_sqd_bsize;	/* SQ data buffer size */

	/* SQ's CQ attributes */
	struct flexio_cq_attr sqcq_attr = {
		.log_cq_depth = L2_LOG_CQ_RING_DEPTH,
		/* SQ does not need APU CQ */
		.element_type = FLEXIO_CQ_ELEMENT_TYPE_NON_DPA_CQ,
		.uar_id = flexio_uar_get_id(app_cfg->flexio_uar),
		.uar_base_addr = 0};
	/* SQ attributes */
	struct flexio_wq_attr sq_attr = {
		.log_wq_depth = L2_LOG_SQ_RING_DEPTH,
		.uar_id = flexio_uar_get_id(app_cfg->flexio_uar),
		.pd = app_cfg->pd};

	/* Allocate memory for SQ's CQ */
	result = allocate_cq_memory(app_cfg->flexio_process, L2_LOG_CQ_RING_DEPTH, &app_cfg->sq_cq_transf);
	if (result != DOCA_SUCCESS)
		return result;

	sqcq_attr.cq_dbr_daddr = app_cfg->sq_cq_transf.cq_dbr_daddr;
	sqcq_attr.cq_ring_qmem.daddr = app_cfg->sq_cq_transf.cq_ring_daddr;

	/* Create SQ's CQ */
	ret = flexio_cq_create(app_cfg->flexio_process, app_cfg->ibv_ctx, &sqcq_attr,
			       &app_cfg->flexio_sq_cq_ptr);
	if (ret != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to create FlexIO SQ's CQ");
		return DOCA_ERROR_DRIVER;
	}

	cq_num = flexio_cq_get_cq_num(app_cfg->flexio_sq_cq_ptr);
	app_cfg->sq_cq_transf.cq_num = cq_num;
	app_cfg->sq_cq_transf.log_cq_depth = L2_LOG_CQ_RING_DEPTH;

	/* Allocate memory for SQ */
	log_sqd_bsize = L2_LOG_WQ_DATA_ENTRY_BSIZE + L2_LOG_SQ_RING_DEPTH;
	result = allocate_sq_memory(app_cfg->flexio_process, L2_LOG_SQ_RING_DEPTH, log_sqd_bsize, &app_cfg->sq_transf);
	if (result != DOCA_SUCCESS)
		return result;

	sq_attr.wq_ring_qmem.daddr = app_cfg->sq_transf.wq_ring_daddr;

	ret =  flexio_sq_create(app_cfg->flexio_process, NULL, cq_num, &sq_attr, &app_cfg->flexio_sq_ptr);
	if (ret != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to create FlexIO SQ");
		return DOCA_ERROR_DRIVER;
	}

	app_cfg->sq_transf.wq_num = flexio_sq_get_wq_num(app_cfg->flexio_sq_ptr);
	/* Create SQ TX MKey */
	result = create_dpa_mkey(app_cfg->flexio_process, app_cfg->pd, app_cfg->sq_transf.wqd_daddr,
			  log_sqd_bsize, IBV_ACCESS_LOCAL_WRITE, &app_cfg->sqd_mkey);
	if (result != DOCA_SUCCESS)
		return result;

	app_cfg->sq_transf.wqd_mkey_id = flexio_mkey_get_id(app_cfg->sqd_mkey);

	return DOCA_SUCCESS;

}

/*
 * Initialize WQEs on the RQ
 *
 * @process [in]: FlexIO process
 * @ring_daddr [in]: device address of the RQ ring
 * @log_depth [in]: log2 of the RQ ring size
 * @data_daddr [in]: device address of the RQ data buffer
 * @log_chunk_bsize [in]: log2 of the RQ data buffer chunk size
 * @wqd_mkey_id [in]: WQD MKey ID
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
init_dpa_rq_ring(struct flexio_process *process, flexio_uintptr_t ring_daddr,
			  int log_depth, flexio_uintptr_t data_daddr, int log_chunk_bsize, uint32_t wqd_mkey_id)
{
	struct mlx5_wqe_data_seg *rx_wqes, *dseg;
	size_t data_chunk_bsize;
	size_t ring_bsize;
	int num_of_wqes;
	doca_error_t result = DOCA_SUCCESS;
	int i;

	num_of_wqes = LOG2VALUE(log_depth);
	ring_bsize = num_of_wqes * sizeof(struct mlx5_wqe_data_seg);
	data_chunk_bsize = LOG2VALUE(log_chunk_bsize);

	rx_wqes = calloc(num_of_wqes, sizeof(struct mlx5_wqe_data_seg));

	if (rx_wqes == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory for RQ WQEs");
		return DOCA_ERROR_NO_MEMORY;
	}

	/* Initialize WQEs' data segment */
	dseg = rx_wqes;

	for (i = 0; i < num_of_wqes; i++) {
		mlx5dv_set_data_seg(dseg, data_chunk_bsize, wqd_mkey_id, data_daddr);
		dseg++;
		data_daddr += data_chunk_bsize;
	}

	/* Copy RX WQEs from host to FlexIO RQ ring */
	if (flexio_host2dev_memcpy(process, rx_wqes, ring_bsize, ring_daddr) != FLEXIO_STATUS_SUCCESS)
		result = DOCA_ERROR_DRIVER;

	free(rx_wqes);
	return result;
}

/*
 * Allocate RQ and CQ for the device
 *
 * @app_cfg [in]: application configuration
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_rq(struct l2_reflector_config *app_cfg)
{
	doca_error_t result;
	flexio_status ret;
	uint32_t mkey_id;
	uint32_t cq_num;	/* CQ number */
	uint32_t wq_num;	/* WQ number */
	uint32_t log_rqd_bsize;	/* SQ data buffer size */

	/* RQ's CQ attributes */
	struct flexio_cq_attr rqcq_attr = {
		.log_cq_depth = L2_LOG_CQ_RING_DEPTH,
		.element_type = FLEXIO_CQ_ELEMENT_TYPE_DPA_THREAD,
		.thread = flexio_event_handler_get_thread(app_cfg->event_handler),
		.uar_id = flexio_uar_get_id(app_cfg->flexio_uar),
		.uar_base_addr = 0};
	/* RQ attributes */
	struct flexio_wq_attr rq_attr = {
		.log_wq_depth = L2_LOG_RQ_RING_DEPTH,
		.pd = app_cfg->pd};

	/* Allocate memory for RQ's CQ */
	result = allocate_cq_memory(app_cfg->flexio_process, L2_LOG_CQ_RING_DEPTH, &app_cfg->rq_cq_transf);
	if (result != DOCA_SUCCESS)
		return result;

	rqcq_attr.cq_dbr_daddr = app_cfg->rq_cq_transf.cq_dbr_daddr;
	rqcq_attr.cq_ring_qmem.daddr = app_cfg->rq_cq_transf.cq_ring_daddr;

	/* Create CQ and RQ */
	ret = flexio_cq_create(app_cfg->flexio_process, NULL, &rqcq_attr, &app_cfg->flexio_rq_cq_ptr);
	if (ret != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to create FlexIO RQ's CQ");
		return DOCA_ERROR_DRIVER;
	}

	cq_num = flexio_cq_get_cq_num(app_cfg->flexio_rq_cq_ptr);
	app_cfg->rq_cq_transf.cq_num = cq_num;
	app_cfg->rq_cq_transf.log_cq_depth = L2_LOG_RQ_RING_DEPTH;

	log_rqd_bsize = L2_LOG_RQ_RING_DEPTH + L2_LOG_WQ_DATA_ENTRY_BSIZE;

	flexio_buf_dev_alloc(app_cfg->flexio_process, LOG2VALUE(log_rqd_bsize), &app_cfg->rq_transf.wqd_daddr);
	if (app_cfg->rq_transf.wqd_daddr == 0) {
		DOCA_LOG_ERR("Failed to allocate memory for RQ data buffer");
		return DOCA_ERROR_DRIVER;
	}

	flexio_buf_dev_alloc(app_cfg->flexio_process, LOG2VALUE(L2_LOG_CQ_RING_DEPTH) * sizeof(struct mlx5_wqe_data_seg),
				&app_cfg->rq_transf.wq_ring_daddr);
	if (app_cfg->rq_transf.wq_ring_daddr == 0x0) {
		DOCA_LOG_ERR("Failed to allocate memory for RQ ring buffer");
		return DOCA_ERROR_DRIVER;
	}

	result = allocate_dbr(app_cfg->flexio_process, &app_cfg->rq_transf.wq_dbr_daddr);
	if (result != DOCA_SUCCESS)
		return result;

	/* Create an MKey for RX buffer */
	result = create_dpa_mkey(app_cfg->flexio_process,
						app_cfg->pd,
						app_cfg->rq_transf.wqd_daddr,
						log_rqd_bsize,
						IBV_ACCESS_LOCAL_WRITE,
						&app_cfg->rqd_mkey);
	if (result != DOCA_SUCCESS)
		return result;

	mkey_id = flexio_mkey_get_id(app_cfg->rqd_mkey);

	result = init_dpa_rq_ring(app_cfg->flexio_process, app_cfg->rq_transf.wq_ring_daddr,
				  L2_LOG_CQ_RING_DEPTH, app_cfg->rq_transf.wqd_daddr,
				  L2_LOG_WQ_DATA_ENTRY_BSIZE, mkey_id);
	if (result != DOCA_SUCCESS)
		return result;

	rq_attr.wq_dbr_qmem.memtype = FLEXIO_MEMTYPE_DPA;
	rq_attr.wq_dbr_qmem.daddr = app_cfg->rq_transf.wq_dbr_daddr;
	rq_attr.wq_ring_qmem.daddr = app_cfg->rq_transf.wq_ring_daddr;

	ret = flexio_rq_create(app_cfg->flexio_process, NULL, cq_num, &rq_attr, &app_cfg->flexio_rq_ptr);
	if (ret != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to create FlexIO SQ");
		return DOCA_ERROR_DRIVER;
	}

	wq_num = flexio_rq_get_wq_num(app_cfg->flexio_rq_ptr);
	app_cfg->rq_transf.wqd_mkey_id = mkey_id;
	app_cfg->rq_transf.wq_num = wq_num;

	/* Modify RQ's DBR record to count for the number of WQEs */
	__be32 dbr[2];
	uint32_t rcv_counter = LOG2VALUE(L2_LOG_RQ_RING_DEPTH);
	uint32_t send_counter = 0;

	dbr[0] = htobe32(rcv_counter & 0xffff);
	dbr[1] = htobe32(send_counter & 0xffff);

	ret = flexio_host2dev_memcpy(app_cfg->flexio_process, dbr, sizeof(dbr), app_cfg->rq_transf.wq_dbr_daddr);
	if (ret != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Failed to modify RQ's DBR");
		return DOCA_ERROR_DRIVER;
	}

	return DOCA_SUCCESS;
}

/*
 * Destroy RQ
 *
 * @app_cfg [in]: application configuration
 */
static void
l2_reflector_rq_destroy(struct l2_reflector_config *app_cfg)
{
	flexio_status ret = FLEXIO_STATUS_SUCCESS;

	ret |= flexio_rq_destroy(app_cfg->flexio_rq_ptr);
	ret |= flexio_device_mkey_destroy(app_cfg->rqd_mkey);
	ret |= flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->rq_transf.wq_dbr_daddr);
	ret |= flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->rq_transf.wq_ring_daddr);
	ret |= flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->rq_transf.wqd_daddr);

	if (ret != FLEXIO_STATUS_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy RQ");
}

/*
 * Destroy SQ
 *
 * @app_cfg [in]: application configuration
 */
static void
l2_reflector_sq_destroy(struct l2_reflector_config *app_cfg)
{
	flexio_status ret = FLEXIO_STATUS_SUCCESS;

	ret |= flexio_sq_destroy(app_cfg->flexio_sq_ptr);
	ret |= flexio_device_mkey_destroy(app_cfg->sqd_mkey);
	ret |= flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->sq_transf.wq_dbr_daddr);
	ret |= flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->sq_transf.wq_ring_daddr);
	ret |= flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->sq_transf.wqd_daddr);

	if (ret != FLEXIO_STATUS_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy SQ");
}

/*
 * Destroy CQ
 *
 * @flexio_process [in]: application configuration
 * @cq [in]: FlexIO CQ
 * @cq_transf [in]: CQ resource
 */
static void
l2_reflector_cq_destroy(struct flexio_process *flexio_process, struct flexio_cq *cq, struct app_transfer_cq cq_transf)
{
	flexio_status ret = FLEXIO_STATUS_SUCCESS;

	ret |= flexio_cq_destroy(cq);
	ret |= flexio_buf_dev_free(flexio_process, cq_transf.cq_ring_daddr);
	ret |= flexio_buf_dev_free(flexio_process, cq_transf.cq_dbr_daddr);

	if (ret != FLEXIO_STATUS_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy CQ");
}

/*
 * Destroy WQs and CQs
 *
 * @app_cfg [in]: application configuration
 */
static void
dev_queues_destroy(struct l2_reflector_config *app_cfg)
{
	l2_reflector_rq_destroy(app_cfg);
	l2_reflector_sq_destroy(app_cfg);
	l2_reflector_cq_destroy(app_cfg->flexio_process, app_cfg->flexio_rq_cq_ptr, app_cfg->rq_cq_transf);
	l2_reflector_cq_destroy(app_cfg->flexio_process, app_cfg->flexio_sq_cq_ptr, app_cfg->sq_cq_transf);
}

doca_error_t
l2_reflector_allocate_device_resources(struct l2_reflector_config *app_cfg)
{
	doca_error_t result;
	flexio_status ret;

	result = allocate_sq(app_cfg);
	if (result != DOCA_SUCCESS)
		return result;

	result = allocate_rq(app_cfg);
	if (result != DOCA_SUCCESS)
		return result;

	app_cfg->dev_data = (struct l2_reflector_data *) calloc(1, sizeof(*app_cfg->dev_data));
	if (app_cfg->dev_data == NULL) {
		DOCA_LOG_ERR("Could not allocate application data memory");
		dev_queues_destroy(app_cfg);
		return DOCA_ERROR_NO_MEMORY;
	}

	app_cfg->dev_data->sq_cq_data = app_cfg->sq_cq_transf;
	app_cfg->dev_data->sq_data = app_cfg->sq_transf;
	app_cfg->dev_data->rq_cq_data = app_cfg->rq_cq_transf;
	app_cfg->dev_data->rq_data = app_cfg->rq_transf;

	ret = flexio_copy_from_host(app_cfg->flexio_process, app_cfg->dev_data, sizeof(*app_cfg->dev_data),
				     &app_cfg->dev_data_daddr);
	if (ret != FLEXIO_STATUS_SUCCESS) {
		DOCA_LOG_ERR("Could not copy data to device");
		dev_queues_destroy(app_cfg);
		free(app_cfg->dev_data);
		return DOCA_ERROR_DRIVER;
	}
	return DOCA_SUCCESS;
}

/*
 * Create flow table with single matcher
 *
 * @domain [in]: DR Table domain
 * @level [in]: Table level
 * @priority [in]: Matcher priority
 * @match_mask [in]: Matcher match mask
 * @tbl_out [out]: Output created table and matcher to dr_flow_table
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_flow_table(struct mlx5dv_dr_domain *domain, int level, int priority,
		 struct mlx5dv_flow_match_parameters *match_mask, struct dr_flow_table **tbl_out)
{
	uint8_t criteria_enable = 0x1; /* Criteria enabled  */
	struct dr_flow_table *tbl;
	doca_error_t result;

	tbl = calloc(1, sizeof(*tbl));
	if (tbl == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory for dr table");
		return DOCA_ERROR_NO_MEMORY;
	}

	tbl->dr_table = mlx5dv_dr_table_create(domain, level);
	if (tbl->dr_table == NULL) {
		DOCA_LOG_ERR("Failed to create table [%d]", errno);
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	tbl->dr_matcher = mlx5dv_dr_matcher_create(tbl->dr_table, priority, criteria_enable, match_mask);
	if (tbl->dr_matcher == NULL) {
		DOCA_LOG_ERR("Failed to create matcher [%d]", errno);
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	*tbl_out = tbl;
	return DOCA_SUCCESS;
exit_with_error:
	if (tbl->dr_matcher)
		mlx5dv_dr_matcher_destroy(tbl->dr_matcher);
	if (tbl->dr_table)
		mlx5dv_dr_table_destroy(tbl->dr_table);
	free(tbl);
	return result;
}

/*
 * Destroy DR table with its matcher
 *
 * @tbl [in]: application configuration
 */
static void
destroy_table(struct dr_flow_table *tbl)
{
	if (!tbl)
		return;
	if (tbl->dr_table)
		mlx5dv_dr_table_destroy(tbl->dr_table);
	if (tbl->dr_matcher)
		mlx5dv_dr_matcher_destroy(tbl->dr_matcher);
	free(tbl);
}

/*
 * Destroy DR rule with its action
 *
 * @rule [in]: application configuration
 */
static void
destroy_rule(struct dr_flow_rule *rule)
{
	if (!rule)
		return;
	if (rule->dr_action)
		mlx5dv_dr_action_destroy(rule->dr_action);
	if (rule->dr_rule)
		mlx5dv_dr_rule_destroy(rule->dr_rule);
	free(rule);
}


doca_error_t
l2_reflector_create_steering_rule_rx(struct l2_reflector_config *app_cfg)
{
	struct mlx5dv_flow_match_parameters *match_mask;
	struct mlx5dv_dr_action *actions[1];
	const int actions_len = 1;
	size_t flow_match_size;
	doca_error_t result;

	flow_match_size = sizeof(*match_mask) + MATCH_SIZE;
	match_mask = (struct mlx5dv_flow_match_parameters *) calloc(1, flow_match_size);
	if (match_mask == NULL) {
		DOCA_LOG_ERR("Failed to allocate match mask");
		return DOCA_ERROR_NO_MEMORY;
	}
	match_mask->match_sz = MATCH_SIZE;
	/* Fill match mask, match on all source mac bits */
	DEVX_SET(dr_match_spec, match_mask->match_buf, smac_47_16, 0xffffffff);
	DEVX_SET(dr_match_spec, match_mask->match_buf, smac_15_0, 0xffff);
	app_cfg->rx_domain = mlx5dv_dr_domain_create(app_cfg->ibv_ctx, MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
	if (app_cfg->rx_domain == NULL) {
		DOCA_LOG_ERR("Failed to allocate RX domain [%d]", errno);
		free(match_mask);
		return DOCA_ERROR_DRIVER;
	}

	result = create_flow_table(app_cfg->rx_domain,
				0, /* Table level */
				0, /* Matcher priority */
				match_mask,
				&app_cfg->rx_flow_table);

	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create RX flow table");
		mlx5dv_dr_domain_destroy(app_cfg->rx_domain);
		free(match_mask);
		return result;
	}

	/* Create rule */
	app_cfg->rx_rule = calloc(1, sizeof(*app_cfg->rx_rule));
	if (app_cfg->rx_rule == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory");
		result = DOCA_ERROR_NO_MEMORY;
		goto exit_with_error;
	}

	/* Action = forward to FlexIO RQ */
	app_cfg->rx_rule->dr_action =
		mlx5dv_dr_action_create_dest_devx_tir(flexio_rq_get_tir(app_cfg->flexio_rq_ptr));
	if (app_cfg->rx_rule->dr_action == NULL) {
		DOCA_LOG_ERR("Failed to create RX rule action [%d]", errno);
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	actions[0] = app_cfg->rx_rule->dr_action;
	/* Fill rule match, match on source mac address with this value */
	DEVX_SET(dr_match_spec, match_mask->match_buf, smac_47_16, SRC_MAC >> 16);
	DEVX_SET(dr_match_spec, match_mask->match_buf, smac_15_0, SRC_MAC % (1 << 16));
	app_cfg->rx_rule->dr_rule =
		mlx5dv_dr_rule_create(app_cfg->rx_flow_table->dr_matcher, match_mask, actions_len, actions);
	if (app_cfg->rx_rule->dr_rule == NULL) {
		DOCA_LOG_ERR("Failed to create RX rule [%d]", errno);
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}
	free(match_mask);
	return DOCA_SUCCESS;

exit_with_error:
	free(match_mask);
	if (app_cfg->rx_rule) {
		destroy_rule(app_cfg->rx_rule);
		app_cfg->rx_rule = NULL;
	}
	destroy_table(app_cfg->rx_flow_table);
	app_cfg->rx_flow_table = NULL;
	mlx5dv_dr_domain_destroy(app_cfg->rx_domain);
	return result;

}

doca_error_t
l2_reflector_create_steering_rule_tx(struct l2_reflector_config *app_cfg)
{
	struct mlx5dv_flow_match_parameters *match_mask;
	struct mlx5dv_dr_action *actions[1];
	size_t flow_match_size;
	doca_error_t result;

	flow_match_size = sizeof(*match_mask) + MATCH_SIZE;
	match_mask = calloc(1, flow_match_size);
	if (match_mask == NULL) {
		DOCA_LOG_ERR("Failed to allocate match mask");
		return DOCA_ERROR_NO_MEMORY;
	}
	match_mask->match_sz = MATCH_SIZE;
	/* Fill match mask, match on all destination mac bits */
	DEVX_SET(dr_match_spec, match_mask->match_buf, dmac_47_16, 0xffffffff);
	DEVX_SET(dr_match_spec, match_mask->match_buf, dmac_15_0, 0xffff);
	app_cfg->fdb_domain = mlx5dv_dr_domain_create(app_cfg->ibv_ctx, MLX5DV_DR_DOMAIN_TYPE_FDB);
	if (app_cfg->fdb_domain == NULL) {
		DOCA_LOG_ERR("Failed to allocate FDB domain");
		free(match_mask);
		return DOCA_ERROR_DRIVER;
	}

	result = create_flow_table(app_cfg->fdb_domain,
				0, /* Table level */
				0, /* Matcher priority */
				match_mask,
				&app_cfg->tx_flow_root_table);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create TX root flow table");
		mlx5dv_dr_domain_destroy(app_cfg->fdb_domain);
		free(match_mask);
		return result;
	}

	result = create_flow_table(app_cfg->fdb_domain,
				1, /* Table level */
				0, /* Matcher priority */
				match_mask,
				&app_cfg->tx_flow_table);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create Tx flow table");
		destroy_table(app_cfg->tx_flow_root_table);
		app_cfg->tx_flow_root_table = NULL;
		mlx5dv_dr_domain_destroy(app_cfg->fdb_domain);
		free(match_mask);
		return result;
	}

	/* Jump to entry table rule */
	app_cfg->tx_root_rule = calloc(1, sizeof(*app_cfg->tx_root_rule));
	if (app_cfg->tx_root_rule == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory");
		result = DOCA_ERROR_NO_MEMORY;
		goto exit_with_error;
	}

	app_cfg->tx_root_rule->dr_action = mlx5dv_dr_action_create_dest_table(app_cfg->tx_flow_table->dr_table);
	if (app_cfg->tx_root_rule->dr_action == NULL) {
		DOCA_LOG_ERR("Failed to create action jump to table");
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	actions[0] = app_cfg->tx_root_rule->dr_action;
	/* Fill rule match, match on destination mac address with this value */
	DEVX_SET(dr_match_spec, match_mask->match_buf, dmac_47_16, SRC_MAC >> 16);
	DEVX_SET(dr_match_spec, match_mask->match_buf, dmac_15_0, SRC_MAC % (1 << 16));
	app_cfg->tx_root_rule->dr_rule = mlx5dv_dr_rule_create(app_cfg->tx_flow_root_table->dr_matcher,
								match_mask,
								1, actions);
	if (app_cfg->tx_root_rule->dr_rule == NULL) {
		DOCA_LOG_ERR("Failed to create rule jump to table");
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	/* Send to wire rule */
	app_cfg->tx_rule = calloc(1, sizeof(*app_cfg->tx_rule));
	if (app_cfg->tx_rule == NULL) {
		DOCA_LOG_ERR("Failed to allocate memory");
		result = DOCA_ERROR_NO_MEMORY;
		goto exit_with_error;
	}

	app_cfg->tx_rule->dr_action = mlx5dv_dr_action_create_dest_vport(app_cfg->fdb_domain, 0xFFFF);
	if (app_cfg->tx_rule->dr_action == NULL) {
		DOCA_LOG_ERR("Failed to create action dest vport");
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	actions[0] = app_cfg->tx_rule->dr_action;
	DEVX_SET(dr_match_spec, match_mask->match_buf, dmac_47_16, SRC_MAC >> 16);
	DEVX_SET(dr_match_spec, match_mask->match_buf, dmac_15_0, SRC_MAC % (1 << 16));
	app_cfg->tx_rule->dr_rule = mlx5dv_dr_rule_create(app_cfg->tx_flow_table->dr_matcher,
								match_mask,
								1, actions);
	if (app_cfg->tx_rule->dr_rule == NULL) {
		DOCA_LOG_ERR("Failed to create rule dest vport");
		result = DOCA_ERROR_DRIVER;
		goto exit_with_error;
	}

	free(match_mask);
	return DOCA_SUCCESS;

exit_with_error:
	free(match_mask);
	if (app_cfg->tx_root_rule) {
		destroy_rule(app_cfg->tx_root_rule);
		app_cfg->tx_root_rule = NULL;
	}
	if (app_cfg->tx_rule) {
		destroy_rule(app_cfg->rx_rule);
		app_cfg->tx_rule = NULL;
	}
	destroy_table(app_cfg->tx_flow_root_table);
	app_cfg->tx_flow_root_table = NULL;
	destroy_table(app_cfg->tx_flow_table);
	app_cfg->tx_flow_table = NULL;
	mlx5dv_dr_domain_destroy(app_cfg->fdb_domain);
	return result;
}

void
l2_reflector_device_resources_destroy(struct l2_reflector_config *app_cfg)
{
	dev_queues_destroy(app_cfg);
	flexio_buf_dev_free(app_cfg->flexio_process, app_cfg->dev_data_daddr);
	free(app_cfg->dev_data);
}

void
l2_reflector_steering_rules_destroy(struct l2_reflector_config *app_cfg)
{
	if (app_cfg->rx_rule) {
		destroy_rule(app_cfg->rx_rule);
		app_cfg->rx_rule = NULL;
	}
	if (app_cfg->tx_rule) {
		destroy_rule(app_cfg->tx_rule);
		app_cfg->tx_rule = NULL;
	}
	if (app_cfg->tx_root_rule) {
		destroy_rule(app_cfg->tx_root_rule);
		app_cfg->tx_root_rule = NULL;
	}
	if (app_cfg->rx_flow_table) {
		destroy_table(app_cfg->rx_flow_table);
		app_cfg->rx_flow_table = NULL;
	}
	if (app_cfg->tx_flow_table) {
		destroy_table(app_cfg->tx_flow_table);
		app_cfg->tx_flow_table = NULL;
	}
	if (app_cfg->tx_flow_root_table) {
		destroy_table(app_cfg->tx_flow_root_table);
		app_cfg->tx_flow_root_table = NULL;
	}
	if (app_cfg->rx_domain)
		mlx5dv_dr_domain_destroy(app_cfg->rx_domain);
	if (app_cfg->fdb_domain)
		mlx5dv_dr_domain_destroy(app_cfg->fdb_domain);
}

void
l2_reflector_device_destroy(struct l2_reflector_config *app_cfg)
{
	flexio_status ret = FLEXIO_STATUS_SUCCESS;

	ret |= flexio_event_handler_destroy(app_cfg->event_handler);

	if (ret != FLEXIO_STATUS_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy FlexIO device");
}

void
l2_reflector_ibv_device_destroy(struct l2_reflector_config *app_cfg)
{
	ibv_dealloc_pd(app_cfg->pd);
}

void
l2_reflector_destroy(struct l2_reflector_config *app_cfg)
{
	/* Destroy matcher and rule */
	l2_reflector_steering_rules_destroy(app_cfg);

	/* Destroy WQs */
	l2_reflector_device_resources_destroy(app_cfg);

	/* Destroy FlexIO resources */
	l2_reflector_device_destroy(app_cfg);

	/* Destroy DevX and IBV resources */
	l2_reflector_ibv_device_destroy(app_cfg);

	/* Destroy FlexIO process */
	if (flexio_process_destroy(app_cfg->flexio_process) != FLEXIO_STATUS_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy FlexIO process");

	/* Close ib device */
	ibv_close_device(app_cfg->ibv_ctx);
}

/*
 * ARGP Callback - Handle device parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
device_callback(void *param, void *config)
{
	struct l2_reflector_config *app_cfg = (struct l2_reflector_config *)config;
	char *device_name = (char *)param;
	size_t len;

	len = strnlen(device_name, DOCA_DEVINFO_IBDEV_NAME_SIZE);
	if (len == DOCA_DEVINFO_IBDEV_NAME_SIZE) {
		DOCA_LOG_ERR("Entered IB device name exceeding the maximum size of %d",
				DOCA_DEVINFO_IBDEV_NAME_SIZE - 1);
		return DOCA_ERROR_INVALID_VALUE;
	}
	strlcpy(app_cfg->device_name, device_name, len + 1);

	return DOCA_SUCCESS;
}

doca_error_t
register_l2_reflector_params(void)
{
	struct doca_argp_param *device_param;
	doca_error_t result;

	result = doca_argp_param_create(&device_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_error_get_descr(result));
		return result;
	}
	doca_argp_param_set_short_name(device_param, "d");
	doca_argp_param_set_long_name(device_param, "device");
	doca_argp_param_set_arguments(device_param, "<device name>");
	doca_argp_param_set_description(device_param, "Device name");
	doca_argp_param_set_callback(device_param, device_callback);
	doca_argp_param_set_type(device_param, DOCA_ARGP_TYPE_STRING);
	doca_argp_param_set_mandatory(device_param);
	result = doca_argp_register_param(device_param);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program param: %s", doca_error_get_descr(result));
		return result;
	}

	result = doca_argp_register_version_callback(sdk_version_callback);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register version callback: %s", doca_error_get_descr(result));
		return result;
	}
	return DOCA_SUCCESS;
}

