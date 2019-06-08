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
		pre_request_vote,
		request_vote,
		append_entry,
		heartbeat,

		handle_response_of_request_pre_vote,
		handle_response_of_request_vote,
		handle_response_of_request_heartbeat,

		broadcast_request_vote,
		broadcast_request_heartbeat,

		active_num,

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

	constexpr MessageKey msg_request_vote = MessageKey::request_vote;
	constexpr MessageKey msg_pre_request_vote = MessageKey::pre_request_vote;
	constexpr MessageKey msg_append_entry = MessageKey::append_entry;
	constexpr MessageKey msg_heartbeat = MessageKey::heartbeat;

	constexpr MessageKey msg_broadcast_request_vote = MessageKey::broadcast_request_vote;
	constexpr MessageKey msg_broadcast_request_heartbeat = MessageKey::broadcast_request_heartbeat;

	constexpr MessageKey msg_handle_response_of_request_pre_vote = MessageKey::handle_response_of_request_pre_vote;
	constexpr MessageKey msg_handle_response_of_request_vote = MessageKey::handle_response_of_request_vote;
	constexpr MessageKey msg_handle_response_of_request_heartbeat = MessageKey::handle_response_of_request_heartbeat;

	constexpr MessageKey msg_active_num = MessageKey::active_num;

	constexpr MessageKey msg_pre_vote = MessageKey::pre_vote;
	constexpr MessageKey msg_vote = MessageKey::vote;
	constexpr MessageKey for_test = MessageKey::for_test;
	constexpr MessageKey for_test1 = MessageKey::for_test1;
}