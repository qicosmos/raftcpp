#pragma once
#include <string>
#include <vector>
#include <any>
#include <atomic>
#include <iguana/json.hpp>
#include "entity.h"
#include "log.hpp"

namespace raftcpp {
	struct address {
		std::string ip;
		int port;
	};
	REFLECTION(address, ip, port);

	struct config {
		int host_id;
		address host_addr;
		std::vector<address> peers_addr;
	};
	REFLECTION(config, host_id, host_addr, peers_addr);

	inline constexpr int ELECTION_TIMEOUT = 500;//milliseconds
	inline constexpr int HEARTBEAT_PERIOD = 500 / 2;//milliseconds
}