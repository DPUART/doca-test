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

#include <doca_dpa_dev.h>
#include <doca_dpa_dev_sync_event.h>
/*
 * Kernel function for kernel_launch sample
 */
__dpa_global__ void hello_world(doca_dpa_dev_sync_event_t comp_event1)
{
	uint64_t event_val = 1;
	
	if (comp_event1)
		doca_dpa_dev_sync_event_update_add(comp_event1, event_val);
}
