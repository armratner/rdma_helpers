#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct rdma_host_config {
	std::string hostname;
	std::string ip_address;
	int port;
	std::string device_name;
};

struct rdma_qp_config {
	uint32_t num_qps;
	uint32_t sq_size;
	uint32_t rq_size;
	uint32_t max_inline_data;
	uint32_t max_send_wr;
	uint32_t max_recv_wr;
};

struct rdma_config {
	std::vector<rdma_host_config> hosts;
	rdma_qp_config qp_config;
	uint32_t buffer_size;
	bool use_event_channel;
	uint32_t cq_size;
	uint32_t max_mr_size;
};

class config_parser {
public:
	static rdma_config parse_config(const std::string &config_file) {
		std::ifstream file(config_file);
		if (!file.is_open())
			throw std::runtime_error(
				"Cannot open config file: " + config_file);

		json j;
		file >> j;
		
		rdma_config config;
		
		// Parse hosts
		for (const auto &host : j["hosts"]) {
			rdma_host_config host_config{
				host["hostname"].get<std::string>(),
				host["ip_address"].get<std::string>(),
				host["port"].get<int>(),
				host["device_name"].get<std::string>()
			};
			config.hosts.push_back(host_config);
		}

		// Parse QP configuration
		const auto &qp = j["qp_config"];
		config.qp_config = {
			qp["num_qps"].get<uint32_t>(),
			qp["sq_size"].get<uint32_t>(),
			qp["rq_size"].get<uint32_t>(),
			qp["max_inline_data"].get<uint32_t>(),
			qp["max_send_wr"].get<uint32_t>(),
			qp["max_recv_wr"].get<uint32_t>()
		};

		// Parse general configuration
		config.buffer_size = j["buffer_size"].get<uint32_t>();
		config.use_event_channel = j["use_event_channel"].get<bool>();
		config.cq_size = j["cq_size"].get<uint32_t>();
		config.max_mr_size = j["max_mr_size"].get<uint32_t>();

		return config;
	}
};
