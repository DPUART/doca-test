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

/**
 * @file doca_dpa_dev_rdma.h
 * @page doca dpa rdma
 * @defgroup DPA_RDMA DOCA DPA rdma
 * @ingroup DPA_DEVICE
 * DOCA DPA rdma
 * DOCA RDMA DPA handle to be used within the DPA
 * @{
 */

#ifndef DOCA_DPA_DEV_RDMA_H_
#define DOCA_DPA_DEV_RDMA_H_

#include <doca_dpa_dev.h>
#include <doca_dpa_dev_buf_array.h>
#include <doca_dpa_dev_sync_event.h>

/**
 * @brief declares that we are compiling for the DPA Device
 *
 * @note Must be defined before the first API use/include of DOCA
 */
#define DOCA_DPA_DEVICE

#ifdef __cplusplus
extern "C" {
#endif

/** DPA RDMA handle type definition */
__dpa_global__ typedef uint64_t doca_dpa_dev_rdma_t;

/**
 * @brief Send an RDMA read operation
 *
 * @param [in] rdma
 * RDMA DPA handle
 *
 * @param [in] dst_mem
 * Destination buffer DPA handle
 *
 * @param [in] dst_offset
 * Offset on the destination buffer
 *
 * @param [in] src_mem
 * Source buffer DPA handle
 *
 * @param [in] src_offset
 * Offset on the source buffer
 *
 * @param [in] length
 * Length of buffer
 *
 * @return
 * This function does not return any value
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_rdma_read(doca_dpa_dev_rdma_t rdma,
			    doca_dpa_dev_buf_t dst_mem,
			    uint64_t dst_offset,
			    doca_dpa_dev_buf_t src_mem,
			    uint64_t src_offset,
			    size_t length);

/**
 * @brief Send an RDMA write operation
 *
 * @param [in] rdma
 * RDMA DPA handle
 *
 * @param [in] dst_mem
 * Destination buffer DPA handle
 *
 * @param [in] dst_offset
 * Offset on the destination buffer
 *
 * @param [in] src_mem
 * Source buffer DPA handle
 *
 * @param [in] src_offset
 * Offset on the source buffer
 *
 * @param [in] length
 * Length of buffer
 *
 * @return
 * This function does not return any value
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_rdma_write(doca_dpa_dev_rdma_t rdma,
			     doca_dpa_dev_buf_t dst_mem,
			     uint64_t dst_offset,
			     doca_dpa_dev_buf_t src_mem,
			     uint64_t src_offset,
			     size_t length);

/**
 * @brief Send an RDMA atomic fetch and add operation
 *
 * @param [in] rdma
 * RDMA DPA handle
 *
 * @param [in] dst_mem
 * Destination buffer DPA handle
 *
 * @param [in] dst_offset
 * Offset on the destination buffer
 *
 * @param [in] value
 * Value to add to the destination buffer
 *
 * @return
 * This function does not return any value
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_rdma_atomic_fetch_add(doca_dpa_dev_rdma_t rdma,
					doca_dpa_dev_buf_t dst_mem,
					uint64_t dst_offset,
					size_t value);

/**
 * @brief Signal to set a remote sync event count
 *
 * @param [in] rdma
 * RDMA DPA handle
 *
 * @param [in] remote_sync_event
 * remote sync event DPA handle
 *
 * @param [in] count
 * Count to set
 *
 * @return
 * This function does not return any value
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_rdma_signal_set(doca_dpa_dev_rdma_t rdma,
				  doca_dpa_dev_sync_event_remote_net_t remote_sync_event,
				  uint64_t count);

/**
 * @brief Signal to atomically add to a remote sync event count
 *
 * @param [in] rdma
 * RDMA DPA handle
 *
 * @param [in] remote_sync_event
 * remote sync event DPA handle
 *
 * @param [in] count
 * Count to add
 *
 * @return
 * This function does not return any value
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_rdma_signal_add(doca_dpa_dev_rdma_t rdma,
				  doca_dpa_dev_sync_event_remote_net_t remote_sync_event,
				  uint64_t count);

/**
 * @brief Synchronize all operations on an RDMA DPA handle
 *
 * @param [in] rdma
 * RDMA DPA handle
 *
 * @return
 * This function does not return any value
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_rdma_synchronize(doca_dpa_dev_rdma_t rdma);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* DOCA_DPA_DEV_RDMA_H_ */
