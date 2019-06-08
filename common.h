#pragma once
#include <string>
#include <vector>
#include <any>
#include <atomic>
//#include <iguana/json.hpp>
#include "entity.h"

namespace raftcpp {
	struct address {
		std::string ip;
		int port;
		int host_id;
	};
	//REFLECTION(address, ip, port, host_id);

	struct config {
		std::vector<address> peers_addr;
	};
	//REFLECTION(config, peers_addr);

	static const int ELECTION_TIMEOUT = 500;//milliseconds
	static const int VOTE_TIMEOUT = 500;//milliseconds
	static const int RPC_TIMEOUT = 500;
	static const int HEARTBEAT_PERIOD = ELECTION_TIMEOUT / 2;//milliseconds

	enum class MessageKey {
		restart_election_timer,
		election_timeout,
		cacel_election_timer,
		restart_vote_timer,
		vote_timeout,
		cancel_vote_timer,
		restart_heartbeat_timer,
		heartbeat_timeout,
		cancel_heartbeat_timer,
		pre_vote,
		vote,
		for_test,
		for_test1,
	};
	constexpr MessageKey msg_restart_election_timer = MessageKey::restart_election_timer;
	constexpr MessageKey msg_election_timeout = MessageKey::election_timeout;
	constexpr MessageKey msg_cancel_election_timer = MessageKey::cacel_election_timer;

	constexpr MessageKey msg_restart_vote_timer = MessageKey::restart_vote_timer;
	constexpr MessageKey msg_vote_timeout = MessageKey::vote_timeout;
	constexpr MessageKey msg_cancel_vote_timer = MessageKey::cancel_vote_timer;

	constexpr MessageKey msg_restart_heartbeat_timer = MessageKey::restart_heartbeat_timer;
	constexpr MessageKey msg_heartbeat_timeout = MessageKey::heartbeat_timeout;
	constexpr MessageKey msg_cancel_heartbeat_timer = MessageKey::cancel_heartbeat_timer;

	constexpr MessageKey msg_pre_vote = MessageKey::pre_vote;
	constexpr MessageKey msg_vote = MessageKey::vote;
	constexpr MessageKey for_test = MessageKey::for_test;
	constexpr MessageKey for_test1 = MessageKey::for_test1;
}