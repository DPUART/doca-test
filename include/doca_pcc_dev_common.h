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

#ifndef DOCA_PCC_DEV_COMMON_H_
#define DOCA_PCC_DEV_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief API functions return status
 */
typedef enum {
	DOCA_PCC_DEV_STATUS_OK      = 0, /* completed successfully */
	DOCA_PCC_DEV_STATUS_FAIL    = 1, /* Failed */
} doca_pcc_dev_error_t;

/**
 * @brief CC event type
 */
typedef enum {
	DOCA_PCC_DEV_EVNT_NULL       =  0,
	/**< Unspecified event type */
	DOCA_PCC_DEV_EVNT_FW         =  1,
	/**< Deprecated - not used */
	DOCA_PCC_DEV_EVNT_ROCE_CNP   =  2,
	/**< RoCE CNP (Congestion Notification Packet) received */
	DOCA_PCC_DEV_EVNT_ROCE_TX    =  3,
	/**< TX packet burst transition ended */
	DOCA_PCC_DEV_EVNT_ROCE_ACK   =  4,
	/**< RoCE ACK Packet received */
	DOCA_PCC_DEV_EVNT_ROCE_NACK  =  5,
	/**< RoCE NACK Packet received */
	DOCA_PCC_DEV_EVNT_RTT        =  6
	/**< RTT probe response packet event */
} doca_pcc_dev_event_type_enum;

/**
* @brief CC Nack event subtypes
*/
typedef enum {
	DOCA_PCC_DEV_NACK_EVNT_NULL      =  0,
	/**< Unspecified NACK type */
	DOCA_PCC_DEV_NACK_EVNT_RNR       =  1,
	/**< RNR (Receiver Not Ready) NACK received */
	DOCA_PCC_DEV_NACK_EVNT_OOS       =  2,
	/**< OOS (Out of Sequence) NACK received */
	DOCA_PCC_DEV_NACK_EVNT_DUP_READ  =  3,
	/**< Duplicated Read (with same PSN) NACK received */
} doca_pcc_dev_nack_event_sub_type_enum;

/**
 * @brief TX Flag: Ack expected
 */
#define DOCA_PCC_DEV_TX_FLAG_ACK_EXPECTED            (1 << 0)

/**
 * @brief TX Flag: Overloaded:
 */
#define DOCA_PCC_DEV_TX_FLAG_OVERLOADED              (1 << 1)

/**
 * @brief TX Flag: RTT packer sent
 */
#define DOCA_PCC_DEV_TX_FLAG_RTT_REQ_SENT            (1 << 2)

/**
 * @brief defines the fixed point fraction size of the rate limiter
 */
#define DOCA_PCC_DEV_LOG_MAX_RATE                    (20) /* rate format in fixed point 20 */

/**
 * @brief Max rate in rate limiter fixed point
 */
#define DOCA_PCC_DEV_MAX_RATE                        (1U << DOCA_PCC_DEV_LOG_MAX_RATE)

/**
 * @brief Default rate. The user overrides teh default in the user algo function
 */
#define DOCA_PCC_DEV_DEFAULT_RATE                    ((DOCA_PCC_DEV_MAX_RATE >> 8) > (1) ? \
							(DOCA_PCC_DEV_MAX_RATE >> 8) : (1))

/**
 * @brief Max number of NIC ports supported by the lib
 */
#define DOCA_PCC_DEV_MAX_NUM_PORTS                   (4)

/**
 * @brief Max number of algo slots supported by the lib
 */
#define DOCA_PCC_DEV_MAX_NUM_USER_SLOTS              (8)

/**
 * @brief Max number of algos supported by the lib
 */
#define DOCA_PCC_DEV_MAX_NUM_ALGOS                   (8)

/**
 * @brief Max number of paramaters per algo supported by the lib
 */
#define DOCA_PCC_DEV_MAX_NUM_PARAMS_PER_ALGO         (0x1E)

/**
 * @brief Max number of counters per algo supported by the lib
 */
#define DOCA_PCC_DEV_MAX_NUM_COUNTERS_PER_ALGO       (0xF)

/**
 * @brief Reserved algo slot for internal algo provided by the lib.
 */
#define DOCA_PCC_DEV_ALGO_SLOT_INTERNAL              (0xF)

/**
 * @brief Reserved algo index for internal algo provided by the lib.
 */
#define DOCA_PCC_DEV_ALGO_INDEX_INTERNAL              (0xF)

#ifdef __cplusplus
}
#endif

#endif /* DOCA_PCC_DEV_COMMON_H_ */

/** @} */
