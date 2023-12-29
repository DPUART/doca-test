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
#include <stdlib.h>
#include <unistd.h>

#include <doca_argp.h>
#include <doca_log.h>
#include <doca_version.h>

#include "firefly_monitor_core.hpp"
#include "client.hpp"

DOCA_LOG_REGISTER(FIREFLY_MONITOR::MAIN);

/*
 * Register the command line parameters for the service tool.
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
register_firefly_monitor_params(void)
{
	doca_error_t result = DOCA_SUCCESS;

	result = doca_argp_register_version_callback(firefly_monitor_version_callback);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register version callback: %s", doca_error_get_descr(result));
		return result;
	}
	return result;
}

/*
 * Main function for DOCA Firefly's PTP Monitor client
 *
 * @argc [in]: command line arguments size
 * @argv [in]: array of command line arguments
 * @return: EXIT_SUCCESS on success and EXIT_FAILURE otherwise
 */
int
main(int argc, char *argv[])
{
	doca_error_t result;
	const char *grpc_address;

	/* Register a logger backend */
	result = doca_log_backend_create_standard();
	if (result != DOCA_SUCCESS) {
		return EXIT_FAILURE;
	}

	/* Parse cmdline/json arguments */
	result = doca_argp_init("doca_firefly_monitor_client", NULL);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_error_get_descr(result));
		return EXIT_FAILURE;
	}

	doca_argp_set_grpc_program();

	/* Register the monitor params */
	result = register_firefly_monitor_params();
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to register program parameters: %s", doca_error_get_descr(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Start parsing the arguments */
	result = doca_argp_start(argc, argv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to parse program input: %s", doca_error_get_descr(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Print a banner when we start */
	printf("#########################################\n");
	printf("## DOCA Firefly Monitor 2023 By NVIDIA ##\n");
	printf("##           Version:  %6s          ##\n", FIREFLY_MONITOR_VERSION);
	printf("#########################################\n\n");

	if (doca_argp_get_grpc_addr(&grpc_address) != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get grpc address");
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	/* Start the client */
	result = run_client(grpc_address);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Firefly Monitor encountered an error: %s", doca_error_get_descr(result));
		doca_argp_destroy();
		return EXIT_FAILURE;
	}

	DOCA_LOG_INFO("Firefly Monitor finished successfully");
	doca_argp_destroy();

	return EXIT_SUCCESS;
}
