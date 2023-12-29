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

#ifndef CLIENT_H_
#define CLIENT_H_

#include <grpcpp/grpcpp.h>

#include <doca_error.h>

#include "firefly_monitor.grpc.pb.h"

/*
 * Starts the client.
 *
 * @arg [in]: String representing the server IP, i.e. "127.0.0.1" or "192.168.100.3:5050"
 *            If no port is provided, it will use the server's default port
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
run_client(const char *arg);

class FireflyMonitorClient {
public:
	FireflyMonitorClient(std::shared_ptr<grpc::Channel> channel)
		: stub_(FireflyMonitor::FireflyMonitor::NewStub(channel))
	{
	}

	std::unique_ptr<grpc::ClientReader<MonitorRecord>> stream;
	std::unique_ptr<FireflyMonitor::Stub> stub_;
};

#endif /* CLIENT_H_ */
