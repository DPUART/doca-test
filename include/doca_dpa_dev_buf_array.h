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
 * @file doca_dpa_dev_buf_array.h
 * @page doca dpa buf array
 * @defgroup DPA_BUF_ARRAY DOCA DPA buf array
 * @ingroup DPA_DEVICE
 * DOCA DPA buffer array
 * DOCA Buffer Array DPA handle to be used within the DPA
 * @{
 */

#ifndef DOCA_DPA_DEV_BUF_ARRAY_H_
#define DOCA_DPA_DEV_BUF_ARRAY_H_

#include <doca_dpa_dev.h>

/**
 * @brief declares that we are compiling for the DPA Device
 *
 * @note Must be defined before the first API use/include of DOCA
 */
#define DOCA_DPA_DEVICE

#ifdef __cplusplus
extern "C" {
#endif

/** DPA buffer array handle type definition */
__dpa_global__ typedef uint64_t doca_dpa_dev_buf_arr_t;

/**
 * @brief Get DPA buffer handle from a DPA buffer array handle
 *
 * @param [in] buf_arr
 * DOCA DPA device buf array handle
 *
 * @param [in] buf_idx
 * DOCA DPA buffer index
 *
 * @return
 * Handle to DPA buffer
 */
DOCA_EXPERIMENTAL
doca_dpa_dev_buf_t doca_dpa_dev_buf_array_get_buf(doca_dpa_dev_buf_arr_t buf_arr, const uint64_t buf_idx);

/**
 * @brief Get address from a DPA buffer handle
 *
 * @param [in] buf
 * DOCA DPA device buf handle
 *
 * @return
 * Address held by DPA buffer
 */
DOCA_EXPERIMENTAL
uintptr_t doca_dpa_dev_buf_get_addr(doca_dpa_dev_buf_t buf);

/**
 * @brief Get length from a DPA buffer handle
 *
 * @param [in] buf
 * DOCA DPA device buf handle
 *
 * @return
 * Length of DPA buffer
 */
DOCA_EXPERIMENTAL
uint64_t doca_dpa_dev_buf_get_len(doca_dpa_dev_buf_t buf);

/**
 * \brief Obtain a pointer to externally allocated memory
 *
 * This function allows the DPA process to obtain a pointer to external memory that is held by a DPA handle.
 * The obtained pointer can be used to load/store data directly from the DPA kernel.
 * The memory being accessed through the returned device pointer is subject to 64B alignment restriction
 *
 * @param [in] buf
 * DOCA DPA device buf handle
 *
 * @param [in] buf_offset
 * Offset of external address being accessed
 *
 * @return
 * Device address pointing to external address
 */
DOCA_EXPERIMENTAL
doca_dpa_dev_uintptr_t doca_dpa_dev_buf_get_external_ptr(doca_dpa_dev_buf_t buf, uint64_t buf_offset);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* DOCA_DPA_DEV_BUF_ARRAY_H_ */
