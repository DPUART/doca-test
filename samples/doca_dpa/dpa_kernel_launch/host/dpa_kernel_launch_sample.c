/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#include <stdlib.h>
#include <unistd.h>

#include <infiniband/mlx5dv.h>

#include <doca_error.h>
#include <doca_log.h>

#include "dpa_common.h"

DOCA_LOG_REGISTER(KERNEL_LAUNCH::SAMPLE);

/* Kernel function decleration */
extern doca_dpa_func_t hello_world;

/*
 * Run kernel_launch sample
 *
 * @resources [in]: DOCA DPA resources that the DPA sample will use
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
typedef struct Timer{
	struct timespec start, end, now;
    size_t elapsed;
}Timer;
Timer record;
void Start(Timer *record) {
	record->elapsed = 0;
    clock_gettime(CLOCK_MONOTONIC, &(record->start));
}
void Stop(Timer *record) {
	clock_gettime(CLOCK_MONOTONIC, &(record->end));
	record->elapsed = ((record->end).tv_sec - (record->start).tv_sec) * 1000000000 + ((record->end).tv_nsec - (record->start).tv_nsec);
}
double GetUSeconds(Timer *record) {
	return record->elapsed / 1000.0;
}
double GetSeconds(Timer *record) {
	return record->elapsed / 1000000000.0;
}


static doca_error_t
create_dpa_events(struct doca_dpa *doca_dpa, struct doca_dev *doca_device, 
		struct doca_sync_event **node_d_comp_event,
		doca_dpa_dev_sync_event_t *node_d_comp_event_handle, int num)
{
	doca_error_t result, tmp_result;
	int i, j;

	for (i = 0; i < num; i++) {

	result = create_doca_dpa_completion_sync_event(doca_dpa, doca_device, &(node_d_comp_event[i]));
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to create host completion event: %s", doca_error_get_descr(result));
			goto destroy_node_d_event;
		}
	doca_sync_event_update_set(node_d_comp_event[i], 0);
	result = doca_sync_event_export_to_dpa(node_d_comp_event[i], doca_dpa, &(node_d_comp_event_handle[i]));
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to export host completion event: %s", doca_error_get_descr(result));
		goto destroy_node_d_event;
	}
	}
	return result;

destroy_node_d_event:
	tmp_result = doca_sync_event_destroy(*node_d_comp_event);
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to destroy node_d_comp_event: %s", doca_error_get_descr(tmp_result));
		DOCA_ERROR_PROPAGATE(result, tmp_result);
	}
	return result;
}

doca_error_t
kernel_launch(struct dpa_resources *resources)
{
	const unsigned int num_dpa_threads = 255;
	struct doca_sync_event *wait_event = NULL;
	doca_dpa_dev_sync_event_t node_d_comp_event_handle = 0;
	struct doca_sync_event *comp_event[num_dpa_threads];
	doca_dpa_dev_sync_event_t node_event_handles[num_dpa_threads];
	/* Wait event threshold */
	uint64_t wait_thresh = 0;
	/* Completion event val */
	uint64_t comp_event_val = 1;
	/* Number of DPA threads */
	
	const unsigned int num_threads = 64;
	doca_error_t result, tmp_result;
	/* Creating DOCA sync event for DPA kernel completion */
	result = create_dpa_events(resources->doca_dpa, resources->doca_device, comp_event, node_event_handles,num_dpa_threads);
	// result = create_doca_dpa_completion_sync_event(resources->doca_dpa, resources->doca_device, &comp_event);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create DOCA sync event for DPA kernel completion: %s", doca_error_get_descr(result));
		return result;
	}
	DOCA_LOG_INFO("All DPA resources have been created");
	Start(&record);

	for (int i = 0; i < num_dpa_threads; i++) {
		result = doca_dpa_kernel_launch_update_add(resources->doca_dpa, wait_event, wait_thresh,
						NULL, 0, num_threads, &hello_world, node_event_handles[i]);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to launch hello_world kernel: %s", doca_error_get_descr(result));
			goto destroy_event;
		}
	}
	result = doca_sync_event_wait_gt(comp_event[num_dpa_threads-1], num_threads - 1, SYNC_EVENT_MASK_FFS);
	if (result != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to wait for host completion event: %s", doca_error_get_descr(result));
	Stop(&record);
	printf("time = %.6fus\n IOPS = %f\n", GetUSeconds(&record),  num_dpa_threads*num_threads / GetUSeconds(&record) * 1000000);
destroy_event:
	/* destroy events */
	for (int i = 0; i < num_dpa_threads; i++) 
	{
		tmp_result = doca_sync_event_destroy(comp_event[i]);
	}
	
	if (tmp_result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to destroy DOCA sync event: %s", doca_error_get_descr(tmp_result));
		DOCA_ERROR_PROPAGATE(result, tmp_result);
	}

	return result;
}
