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

#ifndef FIREFLY_MONITOR_CORE_H_
#define FIREFLY_MONITOR_CORE_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <doca_error.h>

#define FIREFLY_MONITOR_VERSION "1.3.0" /* Firefly's version */

#define INVALID_VALUE_STRING "NA" /* Mark a failure to find the real value */
#define MAX_TIME_STR_LEN 48
#define CANONICAL_ID_LEN 64

typedef enum PTP_STATE {
	STATE_STABLE,	/* PTP is in a stable state */
	STATE_FAULTY,	/* PTP is currently out of sync */
	STATE_RECOVERED /* PTP managed to recover from a sync error */
} ptp_state_t;

typedef enum MONITOR_PORT_STATE {
	MONITOR_PORT_STATE_INACTIVE,	 /* PTP port is in an inactive state */
	MONITOR_PORT_STATE_UNCALIBRATED, /* PTP port is active, but uncalibrated */
	MONITOR_PORT_STATE_ACTIVE,	 /* PTP port is active and calibrated */
} monitor_port_state_t;

struct sampling_value {
	int64_t max;	 /* Maximal (abs) sampled value */
	int64_t average; /* Average value across samples */
	uint64_t rms;	 /* Standard deviation (RMS) of samples */
};

struct timestamp_value {
	uint64_t raw;		       /* Raw timestamp (units vary based on used field) */
	char string[MAX_TIME_STR_LEN]; /* User-friendly timestamp string */
};

/*
 * The set of details we collect regarding the PTP state, IEEE 1588 field info was inspired by the following:
 * https://github.com/YangModels/yang/blob/main/experimental/ieee/1588/ni-ieee1588-ptp.yang
 */
struct ptp_info {
	bool gm_present;			  /* Is the Grandmaster present? */
	ptp_state_t ptp_stability;		  /* Stability state of the PTP */
	struct timestamp_value ptp_time;	  /* The accurate PTP timestamp (nanoseconds) */
	struct timestamp_value adjusted_ptp_time; /* UTC adjusted accurate timestamp (nanoseconds) */
	struct timestamp_value sys_time;	  /* The system's time (seconds) */
	int32_t error_count;			  /* Number of errors we encountered thus far */
	struct timestamp_value last_error_time;	  /* Timestamp for the last encountered error (seconds) */
	char gm_identity[CANONICAL_ID_LEN];	  /* Grandmaster identity (canonicalized string) */
	char port_identity[CANONICAL_ID_LEN];	  /* Our own identity (canonicalized string) */
	struct sampling_value master_offset;	  /* Offset from the master clock (in nano seconds) */
	int64_t utc_offset;			  /* Current offset from UTC (in seconds) */
	bool time_traceable;			  /* PTP timeTraceable property */
	bool frequency_traceable;		  /* PTP frequencyTraceable property */
	uint8_t gm_priority1;			  /* Priority1 field of the Grandmaster clock */
	uint8_t gm_clock_class;			  /* Clock class of the Grandmaster clock */
	uint8_t gm_clock_accuracy;		  /* Clock accuracy of the Grandmaster clock */
	uint8_t gm_priority2;			  /* Priority2 field of the Grandmaster clock */
	uint16_t gm_scaled_offset;		  /* Offset scaled log variance of the Grandmaster clock */
	uint8_t domain_number;			  /* The PTP domainNumber */
	monitor_port_state_t port_state;	  /* The effective PTP port state of the most active port */
	bool tai_timescale;			  /* Are we working in PTP time (TAI) or in UTC? */
};

/*
 * Translate the stability state into a user-facing string
 *
 * @state [in]: Stability state enum
 * @return: User-facing stability state string
 */
const char *
get_stability_string(ptp_state_t state);

/*
 * Translate the monitor port state into a user-facing string
 *
 * @state [in]: Monitor port state state enum
 * @return: User-facing stability state string
 */
const char *
get_port_state_string(monitor_port_state_t state);

/*
 * Report the version of the program and exit.
 *
 * @param [in]: Unused (came from DOCA ARGP)
 * @doca_config [in]: Unused (came from DOCA ARGP)
 * @return: The function exits with EXIT_SUCCESS
 */
[[noreturn]] doca_error_t
firefly_monitor_version_callback(void *param, void *doca_config);

/*
 * Report the results from the monitoring round to the standard output
 *
 * @ptp_state [in]: PTP State to be reported
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
report_monitoring_result_to_stdout(struct ptp_info *ptp_state);

#endif /* FIREFLY_MONITOR_CORE_H_ */
