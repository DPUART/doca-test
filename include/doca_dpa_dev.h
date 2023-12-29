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
 * @defgroup DPA_DEVICE DPA Device
 * DOCA DPA Device library. For more details please refer to the user guide on DOCA devzone.
 *
 * @ingroup DPA
 *
 * @{
 */

#ifndef DOCA_DPA_DEV_H_
#define DOCA_DPA_DEV_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief declares that we are compiling for the DPA Device
 *
 * @note Must be defined before the first API use/include of DOCA
 */
#define DOCA_DPA_DEVICE

/** Include to define compatibility with current version, define experimental Symbols */
#define __linux__
#include <doca_compat.h>
#undef __linux__

#ifdef __cplusplus
extern "C" {
#endif

/** DPA buffer handle type definition */
__dpa_global__ typedef uint64_t doca_dpa_dev_buf_t;
/** DPA pointer type definition */
__dpa_global__ typedef uint64_t doca_dpa_dev_uintptr_t;

/**
 * \brief Obtains the thread rank
 *
 * Retrieves the thread rank for a given kernel on the DPA.
 * The function returns a number in {0..N-1}, where N is the number of threads requested for launch during a kernel
 * submission
 *
 * @return
 * Returns the thread rank.
 */
DOCA_EXPERIMENTAL
unsigned int doca_dpa_dev_thread_rank(void);

/**
 * \brief Obtains the number of threads running the kernel
 *
 * Retrieves the number of threads assigned to a given kernel. This is the value `nthreads` that was passed in to
 * 'doca_dpa_kernel_launch_update_set/doca_dpa_kernel_launch_update_add'
 *
 * @return
 * Returns the number of threads running the kernel
 */
DOCA_EXPERIMENTAL
unsigned int doca_dpa_dev_num_threads(void);

/**
 * \brief Yield a DPA thread
 *
 * This function yields a DPA thread that is running a kernel
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_yield(void);

/**
 * \brief Print to Host
 *
 * This function prints from device to host's standard output stream.
 * Multiple threads may call this routine simultaneously. Printing is a convenience service, and due to limited
 * buffering on the host, not all print statements may appear on the host
 *
 * @param format [in] - Format string that contains the text to be written to stdout (same as from regular printf)
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

/**
 * \brief Initiate a copy data locally from Host
 *
 * This function copies data between two memory regions. The destination buffer, specified by `dest_addr` and `length`
 * will contain the copied data after the memory copy is complete. This is a non-blocking routine
 *
 * @param dst_mem [in] - Destination memory buffer to copy into
 * @param dst_offset [in] - Offset from start address of destination buffer
 * @param src_mem [in] - Source memory buffer to read from
 * @param src_offset [in] - Offset from start address of source buffer
 * @param length [in] - Size of buffer
 *
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_memcpy_nb(doca_dpa_dev_buf_t dst_mem,
			    uint64_t dst_offset,
			    doca_dpa_dev_buf_t src_mem,
			    uint64_t src_offset,
			    size_t length);

/**
 * \brief Initiate a transpose locally from Host
 *
 * This function transposes a 2D array. The destination buffer, specified by `dest_addr` and `length` will contain the
 * copied data after the operation is complete. This is a non-blocking routine
 *
 * @param dst_mem [in] -Destination memory buffer to transpose into
 * @param dst_offset [in] - Offset from start address of destination buffer
 * @param src_mem [in] - Source memory buffer to transpose from
 * @param src_offset [in] - Offset from start address of source buffer
 * @param length [in] - Size of buffer
 * @param element_size [in] - Size of datatype of one element
 * @param num_columns [in] - Number of columns in 2D array
 * @param num_rows [in] - Number of rows in 2D array
 *
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_memcpy_transpose2D_nb(doca_dpa_dev_buf_t dst_mem,
					uint64_t dst_offset,
					doca_dpa_dev_buf_t src_mem,
					uint64_t src_offset,
					size_t length,
					size_t element_size,
					size_t num_columns,
					size_t num_rows);

/**
 * \brief Wait for all memory copy operations issued previously to complete
 *
 * This function returns when memory copy operations issued on this thread have been completed.
 * After this call returns, the buffers they referenced by the copy operations can be reused. This call is blocking
 *
 */
DOCA_EXPERIMENTAL
void doca_dpa_dev_memcpy_synchronize(void);

#ifdef __cplusplus
}
#endif

#endif /* DOCA_DPA_DEV_H_ */

/** @} */
