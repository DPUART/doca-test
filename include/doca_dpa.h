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
 * @defgroup DPA_HOST DPA Host
 * DOCA DPA Host library. For more details please refer to the user guide on DOCA devzone.
 *
 * @ingroup DPA
 *
 * @{
 */
#ifndef DOCA_DPA_H_
#define DOCA_DPA_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <doca_compat.h>
#include <doca_error.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward declarations
 */
struct doca_devinfo;
struct doca_dev;
struct doca_dpa;
struct doca_sync_event;
/** DPA pointer type definition */
typedef uint64_t doca_dpa_dev_uintptr_t;

/**
 * \brief Opaque representation of a DPA Application
 *
 * This is an opaque structure that encapsulates a DPA application.
 * Typically, the DOCA DPA Host application will obtain the value
 * of this structure by linking in the appropriate stub library that is generated by DPACC
 */
struct doca_dpa_app;

/**
 * \brief Generic function pointer type
 *
 * Kernel launches are made using a host function pointer that represents the device function.
 * The host function stub is provided by the associated DPA compiler.
 * The C language does not define conversion of a function pointer to an object pointer (such as void*).
 * Programmers can use this generic function pointer type to typecast to
 * and adhere to strict ISO C language requirements
 */
typedef void (doca_dpa_func_t)(void);

/**
 * \brief Get DPA application name
 *
 * The name of a DPA application is assigned using DPACC during the build phase.
 * Once an application has been formed, its name is embedded within it.
 * This function allows DOCA DPA’s host application to retrieve the name that was previously assigned.
 *
 * The app_name buffer is allocated by the caller along with setting app_name_len indicating the length that was
 * allocated. Upon return the app_name_len field is set to the actual length of the app_name
 *
 * @param app [in] - DPA application generated by DPACC
 * @param app_name [out] - Application name
 * @param app_name_len [in/out] - app_name length. Output is actual number of bytes written
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input, or the buffer received is of insufficient length
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_app_name_get(struct doca_dpa_app *app, char *app_name, size_t *app_name_len);

/**
 * @brief Get whether the DOCA device supports DPA
 *
 * @param devinfo [in] - The device to query
 *
 * @return
 * DOCA_SUCCESS - in case of the DOCA device quered has DPA support
 * Error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_NOT_SUPPORTED - the device quered does not support DPA
 */
DOCA_EXPERIMENTAL
doca_error_t doca_devinfo_get_is_dpa_supported(const struct doca_devinfo *devinfo);

/**
 * \brief Create a DOCA DPA Context
 *
 * This function creates a DOCA DPA context given a DOCA device.
 * The context represents a program on the DPA that is referenced
 * by the host process that called the context creation API
 *
 * @param dev [in] - DOCA device
 * @param app [in] - DPA application generated by DPACC
 * @param dpa [out] - Created context
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 * - DOCA_ERROR_NOT_SUPPORTED - the device does not support DPA
 * - DOCA_ERROR_NO_MEMORY - in case of failure in internal memory allocation
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_create(struct doca_dev *dev, struct doca_dpa_app *app, struct doca_dpa **dpa);

/**
 * \brief Destroy a DOCA DPA context
 *
 * This function destroys DPA context created by doca_dpa_create()
 *
 * @param dpa [in] - Previously created DPA context
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_destroy(struct doca_dpa *dpa);

/**
 * @brief Get maximum number of DPA threads to run a single kernel launch operation
 *
 * @param dpa [in] - DPA context
 * @param value [out] - Number of maximum threads to run a kernel
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * Error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid NULL input
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_get_max_threads_per_kernel(const struct doca_dpa *dpa, unsigned int *value);

/**
 * @brief Get maximum allowable time in seconds that a kernel may remain scheduled on the DPA.
 * A kernel that remains scheduled beyond this limit may be terminated by the runtime and cause fatal behaviour
 *
 * @param dpa [in] - DPA context
 * @param value [out] - maximum allowed time in seconds for a kernel to remain scheduled
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * Error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid NULL input
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_get_kernel_max_run_time(const struct doca_dpa *dpa, unsigned long long *value);

/**
 * @brief Return the last error generated on the DPA.
 * Check if an error occurred on the device side runtime. This call does not reset the error state.
 * If an error occured, the DPA context enters a fatal state and must be destroyed by the user
 *
 * @param dpa [in] - DPA context
 *
 * @return
 * DOCA_SUCCESS - in case of no error state
 * Error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid NULL input
 * - DOCA_ERROR_BAD_STATE - received error on device side
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_peek_at_last_error(const struct doca_dpa *dpa);

/**
 * \brief Submit a kernel to DPA that sets completion event
 *
 * This function submits a kernel for launch on the specified `dpa` context.
 * The kernel starts execution when its wait event value is greater than
 * or equal to specified threshold. The completion event is set to value
 * specified in `comp_count` when the kernel finishes execution.
 *
 * The function to be launched `func` is a host function
 * pointer corresponding to the DPA device function.
 * For example, if the device function is declared as: `__dpa_global__ hello(int arg1)`,
 * then the user is expected to declare the function in the Host application
 * as `extern doca_dpa_func_t hello;`. After the application is linked and loaded
 * using the compiler, a function pointer `hello` can be used in as the `func` argument.
 * The arguments to the function `hello` can be passed inline in the call as var args.
 * For example, to call `hello` on the device using `4` threads with argument `5`,
 * the invocation looks like: `doca_dpa_kernel_launch_update_set(..., 4, hello, 5);`
 *
 * @param dpa [in] - Previously created DPA context
 * @param wait_event [in] - Event to wait on before executing the kernel (optional)
 * @param wait_threshold [in] - Wait event count threshold to wait for before executing. Valid values [0-254]
 * @param comp_event [in] - Event to signal after kernel execution is complete (optional)
 * @param comp_count [in] - Completion count to set for completion event when func is complete
 * @param nthreads [in] - Number of threads to use. This number must be equal or lower than the maximum allowed
 * (see doca_dpa_get_max_threads_per_kernel)
 * @param func [in] - Host function pointer representing DPA kernel
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_kernel_launch_update_set(struct doca_dpa *dpa,
				    struct doca_sync_event *wait_event,
				    uint64_t wait_threshold,
				    struct doca_sync_event *comp_event,
				    uint64_t comp_count,
				    unsigned int nthreads,
				    doca_dpa_func_t *func,
				    ... /* args */);

/**
 * \brief Submit a kernel to DPA
 *
 * This function submits a kernel for launch on the specified `dpa` context.
 * The kernel starts execution when its wait event value is greater than
 * or equal to specified threshold. The value specified in `comp_count`
 * is added to the `comp_event` when the kernel finishes execution.
 *
 * @param dpa [in] - Previously created DPA context
 * @param wait_event [in] - Event to wait on before executing the kernel (optional)
 * @param wait_threshold [in] - Wait event count threshold to wait for before executing. Valid values [0-254]
 * @param comp_event [in] - Event to signal after kernel execution is complete (optional)
 * @param comp_count [in] - Completion count to add for completion event when func is complete
 * @param nthreads [in] - Number of threads to use. This number must be equal or lower than the maximum allowed
 * (see doca_dpa_get_max_threads_per_kernel)
 * @param func [in] - Host function pointer representing DPA kernel
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_kernel_launch_update_add(struct doca_dpa *dpa,
				    struct doca_sync_event *wait_event,
				    uint64_t wait_threshold,
				    struct doca_sync_event *comp_event,
				    uint64_t comp_count,
				    unsigned int nthreads,
				    doca_dpa_func_t *func,
				    ... /* args */);


/**
 * \brief Allocate DPA heap memory
 *
 * This function allocates memory of `size` bytes on the DPA process heap.
 * The memory is aligned for any language supported data type.
 * The memory is not zeroed on allocation. The allocated memory is returned in `dev_ptr` when successful.
 * When memory allocation fails, `dev_ptr` is set to 0x0 (NULL)
 *
 * @param dpa [in] - DPA context
 * @param size [in] - Requested size of allocation
 * @param dev_ptr [out] - Pointer to the allocated memory on the DPA device
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_mem_alloc(struct doca_dpa *dpa, size_t size, doca_dpa_dev_uintptr_t *dev_ptr);

/**
 * \brief Free the previously allocated DPA memory
 *
 * This function frees the allocated memory allocated on the DPA heap. Users are expected to
 * ensure that kernels on the DPA are no longer accessing the memory
 * using established synchronization mechanisms (see events)
 *
 * @param dpa [in] - DPA context
 * @param dev_ptr [in] - Pointer to the memory that was previously allocated on the DPA device
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_mem_free(struct doca_dpa *dpa, doca_dpa_dev_uintptr_t dev_ptr);

/**
 * \brief Copy from host memory to DPA Heap
 *
 * This function copies data from Host memory to the DPA heap. This is a blocking call.
 * When the call returns, the memory on the DPA is set to the values supplied in the Host buffer
 *
 * @param dpa [in] - DPA context
 * @param dst_ptr [in] - DPA device heap destination pointer
 * @param src_ptr [in] - Host source buffer address
 * @param size [in] - Size of data to copy
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_h2d_memcpy(struct doca_dpa *dpa, doca_dpa_dev_uintptr_t dst_ptr, void *src_ptr, size_t size);

/**
 * \brief Copy from DPA Heap to host memory
 *
 * This function copies data from the DPA heap to Host memory. This is a blocking call.
 * When the call returns, the memory on the Host buffer is set to the values supplied in the DPA heap pointer
 *
 * @param dpa [in] - DPA context
 * @param dst_ptr [in] - Host destination buffer address
 * @param src_ptr [in] - DPA device heap sorce pointer
 * @param size [in] - Size of data to copy
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_d2h_memcpy(struct doca_dpa *dpa, void *dst_ptr, doca_dpa_dev_uintptr_t src_ptr, size_t size);

/**
 * \brief Set DPA Heap memory to a value
 *
 * This function sets DPA heap memory to a supplied value. This is a blocking call.
 * When the call returns, the memory on the DPA is set to the value supplied
 *
 * @param dpa [in] - DPA context
 * @param dev_ptr [in] - DPA device heap pointer
 * @param value [in] - Value to set
 * @param size [in] - Size of device buffer
 *
 * @return
 * DOCA_SUCCESS - in case of success
 * doca_error code - in case of failure:
 * - DOCA_ERROR_INVALID_VALUE - received invalid input
 * - DOCA_ERROR_DRIVER - in case of error in a DOCA driver call
 */
DOCA_EXPERIMENTAL
doca_error_t doca_dpa_memset(struct doca_dpa *dpa, doca_dpa_dev_uintptr_t dev_ptr, int value, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DOCA_DPA_H_ */

/** @} */
