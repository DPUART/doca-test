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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>

#include <doca_log.h>

#include "client.hpp"
#include "firefly_monitor_core.hpp"

DOCA_LOG_REGISTER(FIREFLY_MONITOR::GRPC);

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

static volatile bool force_quit;

/*
 * Deserialize a given record from the gRPC structure for it
 *
 * @data [in]: gRPC input to be deserialized
 * @record [out]: record to be populated
 */
static void
deserialize_record(MonitorRecord &data, struct ptp_info *record)
{
	record->gm_present = data.gm_present();
	record->ptp_stability = (enum PTP_STATE)data.ptp_stability();
	record->ptp_time.raw = data.ptp_time().raw();
	strlcpy(record->ptp_time.string, data.ptp_time().str().c_str(), sizeof(record->ptp_time.string));
	record->adjusted_ptp_time.raw = data.adjusted_ptp_time().raw();
	strlcpy(record->adjusted_ptp_time.string, data.adjusted_ptp_time().str().c_str(),
		sizeof(record->adjusted_ptp_time.string));
	record->sys_time.raw = data.sys_time().raw();
	strlcpy(record->sys_time.string, data.sys_time().str().c_str(), sizeof(record->sys_time.string));
	record->error_count = data.error_count();
	record->last_error_time.raw = data.last_error_time().raw();
	strlcpy(record->last_error_time.string, data.last_error_time().str().c_str(),
		sizeof(record->last_error_time.string));
	record->tai_timescale = data.tai_timescale();
	/* The rest of the fields are only sent when needed */
	if (record->gm_present) {
		strlcpy(record->gm_identity, data.gm_identity().c_str(), sizeof(record->gm_identity));
		strlcpy(record->port_identity, data.port_identity().c_str(), sizeof(record->port_identity));
		record->master_offset.max = data.master_offset().max();
		record->master_offset.average = data.master_offset().average();
		record->master_offset.rms = data.master_offset().rms();
		record->utc_offset = data.current_utc_offset();
		record->time_traceable = data.time_traceable();
		record->frequency_traceable = data.frequency_traceable();
		record->gm_priority1 = (uint8_t)data.gm_priority1();
		record->gm_clock_class = data.gm_clock_class();
		record->gm_clock_accuracy = data.gm_clock_accuracy();
		record->gm_priority2 = data.gm_priority2();
		record->gm_scaled_offset = data.gm_offset_scaled_log_variance();
		record->domain_number = data.domain_number();
		record->port_state = (enum MONITOR_PORT_STATE)data.port_state();
	}
}

/*
 * Callback function to handle TERM and INT signals
 *
 * @signum [in]: signal number
 */
static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		DOCA_LOG_INFO("Signal %d received, preparing to exit", signum);
		force_quit = true;
	}
}

doca_error_t
run_client(const char *arg)
{
	doca_error_t result;

	/* Check if we got a port or if we are using the default one */
	std::string server_address(arg);
	if (server_address.find(':') == std::string::npos) {
		server_address += ":" + std::to_string(eNetworkPort::k_DocaFirefly);
	}

	FireflyMonitorClient *client =
		new FireflyMonitorClient(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
	if (client == NULL) {
		DOCA_LOG_ERR("Failed to allocate the gRPC client");
		return DOCA_ERROR_NO_MEMORY;
	}

	/* Subscribe to monitor events */
	grpc::ClientContext context;
	SubscribeReq request;
	client->stream = client->stub_->Subscribe(&context, request);

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Endless monitoring loop */
	while (!force_quit) {
		MonitorRecord grpc_record;
		if (!client->stream->Read(&grpc_record)) {
			DOCA_LOG_ERR("Failed to receive a monitor record from the server");
			delete client;
			return DOCA_ERROR_IO_FAILED;
		}

		struct ptp_info record;
		deserialize_record(grpc_record, &record);
		result = report_monitoring_result_to_stdout(&record);
		if (result != DOCA_SUCCESS) {
			delete client;
			return result;
		}
	}

	delete client;
	return DOCA_SUCCESS;
}
