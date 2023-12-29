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
 * @defgroup PCC_DEVICE PCC Device
 * DOCA PCC Device library. For more details please refer to the user guide on DOCA devzone.
 *
 * @ingroup PCC
 *
 * @{
 */

#ifndef DOCA_PCC_DEV_H_
#define DOCA_PCC_DEV_H_

/**
 * @brief declares that we are compiling for the DPA Device
 *
 * @note Must be defined before the first API use/include of DOCA
 */
#define DOCA_DPA_DEVICE

#include <stddef.h>
#include <stdint.h>
#include <doca_pcc_dev_common.h>
#include <doca_pcc_dev_utils.h>
#include <doca_pcc_dev_data_structures.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Implements the internal CC algorithm provided by the lib
 *
 * The lib provides an internal built-in CC algorithm implementation.
 * The user may call this function for flows with algo_slot
 * that is not set by the user (An unknown algo_slot can be the result of running without algo negotiation)
 *
 * @param[in]  algo_ctxt - @see doca_pcc_dev_user_algo
 * @param[in]  event -     @see doca_pcc_dev_user_algo
 * @param[in]  attr -      @see doca_pcc_dev_user_algo
 * @param[out] results -   @see doca_pcc_dev_user_algo
 *
 * @return void.
 */
void doca_pcc_dev_default_internal_algo(doca_pcc_dev_algo_ctxt_t *algo_ctxt, doca_pcc_dev_event_t *event,
	const doca_pcc_dev_attr_t *attr, doca_pcc_dev_results_t *results);

/**
 * @brief Entry point to the user algorithm handling code
 *
 * This code handles a single event. it recieves the alorithm context,
 * the event information (opaque struct), and some attributes (algo id), and returns
 * the PCC rate
 * The event info should not be used directly through the struct. It is recomended to use
 * the supplied "getter" functions (doca_pcc_dev_event.h) to help generate more future
 * compatible code if event information placement changes
 *
 * @param[in]  algo_ctxt - pointer to user context for this flow (restored from previous iteration)
 * @param[in]  event - pointer to event data struct to be used with getter functions
 * @param[in]  attr - information about event like algo type
 * @param[out]  results - new rate information to be writen to HW.
 *			  The rate is expressed as a 20b fixed point number in range (0 , 1]
 *
 * @return void.
 */
void doca_pcc_dev_user_algo(doca_pcc_dev_algo_ctxt_t *algo_ctxt, doca_pcc_dev_event_t *event,
	const doca_pcc_dev_attr_t *attr, doca_pcc_dev_results_t *results);

/**
 * @brief Entry point to the user one time initialization code
 *
 * This is called on PCC process load and should initialize the data of
 * all user algorithms.
 *
 * @param[out]  disable_event_bitmask - a bitmaks of events that should be discarded and not passed
 * to the event processing code
 *
 * @return void.
 */
void doca_pcc_dev_user_init(uint32_t *disable_event_bitmask);

/**
 * @brief User callback executed then parameters are set.
 *
 * Called when the parameter change was set externally.
 * The implementation should:
 *     Check the given new_parameters values. If those are correct from the algorithm perspective,
 *     assign them to the given parameter array.
 *
 * @param[in]  port_num - index of the port
 * @param[in]  algo_slot - Algo slot identifier as reffered to in the PPCC command field "algo_slot"
 * if possible it should be equal to the algo_idx
 * @param[in]  param_id_base - id of the first parameter that was changed.
 * @param[in]  param_num - number of all parameters that were changed
 * @param[in]  new_param_values - pointer to an array which holds param_num number of new values for parameters
 * @param[in]  params - pointer to an array which holds beginning of the current parameters to be changed
 *
 * @return -
 * DOCA_PCC_DEV_STATUS_OK: Parameters were set
 * DOCA_PCC_DEV_STATUS_FAIL: the values (one or more) are not legal. No parameters were changed
 *
 */
doca_pcc_dev_error_t doca_pcc_dev_user_set_algo_params(uint32_t port_num, uint32_t algo_slot, uint32_t param_id_base,
	uint32_t param_num, const uint32_t *new_param_values, uint32_t *params);

/**
 * @brief Print to Host
 *
 * This function prints from device to host's standard output stream.
 * Multiple threads may call this routine simultaneously. Printing is a convenience service, and due to limited
 * buffering on the host, not all print statements may appear on the host
 *
 * @param[in]  format - Format string that contains the text to be written to stdout (same as from regular printf)
 */
void doca_pcc_dev_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

/**
 * @brief Creates trace message entry with 3 arguments
 *
 * @param[in] format_id -the template format id to print message accordingly
 * @param[in] arg1 - argument #1 to format into the template
 * @param[in] arg2 - argument #2 to format into the template
 * @param[in] arg3 - argument #3 to format into the template
 * @param[in] arg4 - argument #4 to format into the template
 * @param[in] arg5 - argument #5 to format into the template
 *
 * @return void.
 *
 */
void doca_pcc_dev_trace_5(int format_id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

/**
 * @brief Flush the trace message buffer to Host
 *
 * As soon as a buffer is fully occupied it is internally sent to host, however
 * user can ask partially occupied buffer to be sent to host. Its intended use
 * is at end of run to flush whatever messages left.
 *
 * @note: Frequent call to this API might cause performance issues..
 *
 * @return void.
 *
 */
void doca_pcc_dev_trace_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* DOCA_PCC_DEV_H_ */

/** @} */
