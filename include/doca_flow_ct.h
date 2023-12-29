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

/**
 * @file doca_flow_ct.h
 * @page doca flow ct
 * @defgroup Flow ct
 * DOCA HW connection tracking library.
 *
 * @{
 */

#ifndef DOCA_FLOW_CT_H_
#define DOCA_FLOW_CT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <doca_compat.h>
#include <doca_error.h>
#include <doca_flow.h>
#include <doca_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Meta connection type
 */
enum doca_flow_ct_meta_type {
	DOCA_FLOW_CT_META_NONE,    /**< Regular payload traffic */
	DOCA_FLOW_CT_META_NEW,     /**< SYN or first UDP packet */
	DOCA_FLOW_CT_META_END,     /**< FIN or RST packet */
	DOCA_FLOW_CT_META_UPDATE,  /**< Payload to update user action data */
};

/**
 * Meta type mask
 */
#define DOCA_FLOW_CT_META_TYPE_MASK 0x3

/**
 * @brief CT packet meta data
 */
union doca_flow_ct_meta {
	uint32_t u32; /**< CPU endian. */
	struct {
		uint32_t src : 1;      /**< Source port in multi-port E-Switch mode */
		uint32_t hairpin : 1;  /**< Subject to forward using hairpin. */
		uint32_t type : 2;     /**< Refer to doca_flow_ct_meta_type. */
		uint32_t data : 28;    /**< Combination of reserved, zone, user action data and user data. */
	};
};

/**
 * @brief CT flags
 */
enum doca_flow_ct_flags {
	DOCA_FLOW_CT_FLAG_STATS = 1u << 0, /**< Enable counter for internal pipes */
	DOCA_FLOW_CT_FLAG_WORKER_STATS = 1u << 1, /**< Enable worker counter dump */
	DOCA_FLOW_CT_FLAG_NO_AGING = 1u << 2, /**< Bypass aging scan */
	DOCA_FLOW_CT_FLAG_SW_PKT_PARSING = 1u << 3, /**< Force software packet parsing */
	DOCA_FLOW_CT_FLAG_MANAGED = 1u << 4, /**< User managed worker thread, API only */
	DOCA_FLOW_CT_FLAG_ASYMMETRIC = 1u << 5, /**< Asymmetric 6-tuple table definition */
	DOCA_FLOW_CT_FLAG_ASYMMETRIC_COUNTER = 1u << 6, /**< Different counter in both direction */
	DOCA_FLOW_CT_FLAG_NO_COUNTER = 1u << 7, /**< Disable counter support */
	DOCA_FLOW_CT_FLAG_DEFAULT_MISS = 1u << 8, /**< Use the default CT TCP and UDP miss pipes */
};

/**
 * @brief CT l3 session types
 */
enum doca_flow_ct_session_type {
	DOCA_FLOW_CT_SESSION_IPV4,		/**< IPv4 session. */
	DOCA_FLOW_CT_SESSION_IPV6,		/**< IPv6 session. */
	DOCA_FLOW_CT_SESSION_BOTH,		/**< Total session. */
	DOCA_FLOW_CT_SESSION_MAX,		/**< Max session types. */
};

/**
 * @brief CT hash table type.
 */
enum doca_flow_ct_hash_type {
	DOCA_FLOW_CT_HASH_NONE,	/**< No hash table, besides zone, meta data bits reserved as connection ID. */
	DOCA_FLOW_CT_HASH_SYMMETRIC, /**< Hardware symmetric hash function */
};

/**
 * Flow log callback function
 */
typedef void (*doca_flow_ct_flow_log_cb)(struct doca_flow_pipe *pipe, void *entry, void *usr_ctx);

/**
 * Direction config
*/
struct direction_cfg {
	bool match_inner;			                        /**< match packet inner layer */
	struct doca_flow_meta *zone_match_mask;             /**< Zone mask to match */
	struct doca_flow_meta *meta_modify_mask;            /**< meta mask to modify */
};

/**
 * @brief doca flow ct global configuration
 */
struct doca_flow_ct_cfg {
	uint32_t nb_arm_queues; /**< number of ARM CT queues(thread). */
	uint32_t nb_arm_sessions[DOCA_FLOW_CT_SESSION_MAX]; /**< number of ARM CT sessions. */
	uint32_t flags; /**< CT behavior flags */
	struct doca_dev *doca_dev; /**< Doca device, override ib_dev */
	void *ib_dev; /**< IB verbs device context, deprecated */
	void *ib_pd; /**< device protection domain used with DPDK, deprecated */
	uint16_t tcp_timeout_s; /**< TCP timeout in second. */
	uint16_t tcp_session_del_s; /**< time to delay or kill TCP session after RST/FIN. */
	uint16_t udp_timeout_s; /**< UDP timeout in second. */
	uint16_t aging_core; /**< CT aging thread bind to CPU core. */
	doca_flow_ct_flow_log_cb flow_log_cb; /**< Flow log callback function */
	union {
		/* Managed mode: */
		struct direction_cfg direction[2];         /**< Configuration of each direction */
		/* Autonomous mode: */
		struct {
			enum doca_flow_tun_type tunnel_type;   /**< Tunnel type */
			uint16_t vxlan_dst_port;	       /**< outer UDP destination port for VxLAN traffic. BE */
			enum doca_flow_ct_hash_type hash_type; /**< Connection hash table type. */
			uint32_t meta_user_bits;	       /**< User data bits ignored by worker */
			uint32_t meta_action_bits; /**< User action data bits carried by identified connection packet */
			uint32_t meta_zone_bits;   /**< User zone bits carried by identified connection packets */
		};
	}; /* Exclusive configuration */
};

/**
 * @brief Initialize the doca flow ct.
 *
 * This is the global initialization function for doca flow ct. It
 * initializes all resources used by doca flow.
 *
 * Must be invoked first before any other function in this API.
 * this is a one time call, used for doca flow ct initialization and
 * global configurations.
 *
 * Must be invoked after Doca Flow initilization, before port start.
 *
 * @param cfg
 *   CT configuration.
 * @return
 *   0 on success, a negative errno value otherwise.
 */
DOCA_EXPERIMENTAL
doca_error_t
doca_flow_ct_init(const struct doca_flow_ct_cfg *cfg);


/**
 * @brief Sets UDP outer destination port for VxLAN traffic.
 *
 * This is to initialization the UDP outer destination port for VxLAN traffic.
 * Sets the VxLAN dest port global variable value.
 *
 * Optional, default to 4789.
 * Must be invoked after Doca Flow and CT initialization.
 *
 * @param dst_port
 *   outer UDP destination value.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_set_vxlan_dst_port(uint16_t dst_port);

/**
 * @brief Destroy the doca flow ct.
 *
 * Release all the resources used by doca flow ct.
 *
 * Must be invoked before doca flow detroy.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_destroy(void);

/**
 * @brief Prepare meta with zone and default CT type.
 *
 * @param meta
 *   Doca flow meta.
 * @param zone
 *   Zone value.
 * @param is_reply
 *   Prepare reply direction zone in asymmetric mode.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_meta_prepare(struct doca_flow_meta *meta, uint32_t zone, bool is_reply);

/**
 * @brief Prepare meta as mask with zone and CT type.
 *
 * @param meta
 *   Doca flow meta.
 * @param is_reply
 *   Prepare reply direction zone in asymmetric mode.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_meta_mask_prepare(struct doca_flow_meta *meta, bool is_reply);

/**
 * @brief Set meta match zone data to doca_flow meta.
 *
 * @param meta
 *   doca_flow meta.
 * @param zone
 *   Zone value.
 * @param is_reply
 *   Set reply direction zone in asymmetric mode.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_meta_set_match_zone(struct doca_flow_meta *meta, uint32_t zone, bool is_reply);

/**
 * @brief Get modify meta zone data.
 *
 * @param meta
 *   CT meta.
 * @param is_reply
 *   Get reply direction zone in asymmetric mode.
 * @return
 *   Zone value.
 */
DOCA_EXPERIMENTAL
uint32_t
doca_flow_ct_meta_get_zone(uint32_t meta, bool is_reply);

/**
 * @brief Set meta zone data applies to identified connection packets.
 *
 * @param meta
 *   CT meta.
 * @param zone
 *   Zone value.
 * @param is_reply
 *   Set reply direction zone in asymmetric mode.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_meta_set_zone(uint32_t *meta, uint32_t zone, bool is_reply);

/**
 * @brief Set meta action data applies to identified connection packets.
 *
 * @param meta
 *   CT meta.
 * @param action_data
 *   Action data.
 * @param is_reply
 *   Reply direction in asymmetric mode.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_meta_set_action(uint32_t *meta, uint32_t action_data, bool is_reply);

/**
 * @brief Set user data in meta data field.
 *
 * User data is ignored by worker, can't be carried with identified conneciton packets.
 * @param meta
 *   CT meta.
 * @param user_data
 *   User data value.
 * @param is_reply
 *   Reply direction in asymmetric mode.
 */
DOCA_EXPERIMENTAL
void
doca_flow_ct_meta_set_user(uint32_t *meta, uint32_t user_data, bool is_reply);

/**
 * @brief Get zone data bit offset in meta data field.
 *
 * @param is_reply
 *   Reply direction in asymmetric mode.
 * @return
 *   Zone data bit offset.
 */
DOCA_EXPERIMENTAL
uint32_t
doca_flow_ct_meta_get_zone_offset(bool is_reply);

/**
 * @brief Get action data bit offset in meta data field.
 *
 * @param is_reply
 *   Reply direction in asymmetric mode.
 * @return
 *   Action data bit offset.
 */
DOCA_EXPERIMENTAL
uint32_t
doca_flow_ct_meta_get_action_offset(bool is_reply);

/**
 * @brief Get User data bit offset in meta data field.
 *
 * @param is_reply
 *   Reply direction in asymmetric mode.
 * @return
 *   User data bit offset.
 */
DOCA_EXPERIMENTAL
uint32_t
doca_flow_ct_meta_get_user_offset(bool is_reply);

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif /* DOCA_FLOW_CT_H_ */
