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

#include <bsd/string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <doca_error.h>
#include <doca_log.h>

#include "firefly_monitor_core.hpp"

DOCA_LOG_REGISTER(FIREFLY_MONITOR);

const char *
get_stability_string(ptp_state_t state)
{
	switch (state) {
	case STATE_STABLE:
		return "Yes";
	case STATE_FAULTY:
		return "No";
	case STATE_RECOVERED:
		return "Recovered";
	default:
		return INVALID_VALUE_STRING;
	}
}

const char *
get_port_state_string(monitor_port_state_t state)
{
	switch (state) {
	case MONITOR_PORT_STATE_INACTIVE:
		return "Inactive";
	case MONITOR_PORT_STATE_UNCALIBRATED:
		return "Uncalibrated";
	case MONITOR_PORT_STATE_ACTIVE:
		return "Active";
	default:
		return INVALID_VALUE_STRING;
	}
}

[[noreturn]] doca_error_t
firefly_monitor_version_callback(void *param, void *doca_config)
{
	printf("DOCA SDK Version: %s\n", doca_version());
	printf("DOCA Firefly Version: %s\n", FIREFLY_MONITOR_VERSION);
	/* We assume that when printing the versions there is no need to continue the program's execution */
	exit(EXIT_SUCCESS);
}

/*
 * Report the timestamps to the standard output
 *
 * @ptp_state [in]: PTP State to be reported
 * @return: Number of fillter lines required later on
 */
static uint32_t
report_time_to_stdout(struct ptp_info *ptp_state)
{
	uint32_t filler_lines;

	if (ptp_state->tai_timescale) {
		/* ptp_time (TAI):		Thu Sep  1 12:58:19 2022 */
		printf("ptp_time (TAI):            %s\n", ptp_state->ptp_time.string);
		/* ptp_time (UTC adjusted):	Thu Sep  1 12:58:19 2022 */
		printf("ptp_time (UTC adjusted):   %s\n", ptp_state->adjusted_ptp_time.string);
		filler_lines = 0;
	} else {
		/* ptp_time (UTC):		Thu Sep  1 12:58:19 2022 */
		printf("ptp_time (UTC):            %s\n", ptp_state->ptp_time.string);
		filler_lines = 1;
	}
	/* system_time (UTC):		Thu Sep  1 12:58:19 2022 */
	printf("system_time (UTC):         %s\n", ptp_state->sys_time.string);

	return filler_lines;
}

doca_error_t
report_monitoring_result_to_stdout(struct ptp_info *ptp_state)
{
	int i = 0;
	uint32_t filler_lines = 0;

	printf("\n\n");
	if (ptp_state->gm_present) {
		/* gmIdentity:			EC:46:70:FF:FE:10:FE:B9 (ec4670.fffe.10feb9) */
		printf("gmIdentity:                %s\n", ptp_state->gm_identity);
		/* portIdentity:		EC:46:70:FF:FE:10:FE:B9 (ec4670.fffe.10feb9-1) */
		printf("portIdentity:              %s\n", ptp_state->port_identity);
		/* port_state:			Active */
		printf("port_state:                %s\n", get_port_state_string(ptp_state->port_state));
		/* domainNumber:		127 */
		printf("domainNumber:              %u\n", ptp_state->domain_number);
		/* master_offset:		avg:	23 max:	40 rms:	4 */
		printf("master_offset:             avg:\t%ld\tmax:\t%ld\trms:\t%lu\n", ptp_state->master_offset.average,
		       ptp_state->master_offset.max, ptp_state->master_offset.rms);
		/* gmPresent:			true */
		printf("gmPresent:                 true\n");
		/* ptp_stable:			Yes/No/Recovered */
		printf("ptp_stable:                %s\n", get_stability_string(ptp_state->ptp_stability));
		/* currentUtcOffset:		37 */
		printf("UtcOffset:                 %ld\n", ptp_state->utc_offset);
		/* timeTraceable:		1 */
		printf("timeTraceable:             %s\n", (ptp_state->time_traceable ? "1" : "0"));
		/* frequencyTraceable:		1 */
		printf("frequencyTraceable:        %s\n", (ptp_state->frequency_traceable ? "1" : "0"));
		/* grandmasterPriority1:	128 */
		printf("grandmasterPriority1:      %u\n", ptp_state->gm_priority1);
		/* gmClockClass:		6 */
		printf("gmClockClass:              %u\n", ptp_state->gm_clock_class);
		/* gmClockAccuracy:		0x21 */
		printf("gmClockAccuracy:           0x%x\n", ptp_state->gm_clock_accuracy);
		/* grandmasterPriority2:	128 */
		printf("grandmasterPriority2:      %u\n", ptp_state->gm_priority2);
		/* gmOffsetScaledLogVariance:	0x34fb */
		printf("gmOffsetScaledLogVariance: 0x%x\n", ptp_state->gm_scaled_offset);
		filler_lines += report_time_to_stdout(ptp_state);
	} else {
		/* gmPresent:			false */
		printf("gmPresent:                 false\n");
		/* ptp_stable:			Yes/No/Recovered */
		printf("ptp_stable:                %s\n", get_stability_string(ptp_state->ptp_stability));
		filler_lines += report_time_to_stdout(ptp_state);
		/* 15 lines for gmPresent, and 2 that we have here locally */
		filler_lines += 15 - 2;
	}

	if (ptp_state->error_count > 0) {
		printf("error_count:               %u\n", ptp_state->error_count);
		printf("last_err_time (UTC):       %s\n", ptp_state->last_error_time.string);
	} else {
		printf("\n\n");
	}

	/* Maintain the same output length for easy screen formatting */
	for (i = 0; i < filler_lines; i++) {
		printf("\n");
	}

	/* Flush the output to avoid caching */
	fflush(stdout);

	return DOCA_SUCCESS;
}
