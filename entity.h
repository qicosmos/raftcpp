#pragma once
#include <cstdint>
#include <vector>
#include <msgpack.hpp>

namespace raftcpp {
	enum class State {
		FOLLOWER,
		CANDIDATE,
		LEADER
	};
	std::unordered_map<State, std::string> state_to_string{
		{State::FOLLOWER,"Follower"},
		{State::CANDIDATE,"Candidate"},
		{State::LEADER,"Leader"},
	};


	struct entry_t {
		/** the entry's term at the point it was created */
		uint64_t term = 0;

		/** the entry's unique ID */
		uint64_t index = 0;

		/** type of entry */
		int32_t type ;

		std::string data;

		uint64_t req_id = 0;
		MSGPACK_DEFINE(term, index, type, data,req_id)
	};

	typedef std::function<void()> task_callback_t;
	struct task_t {
		entry_t entry;
		task_callback_t callback;
		uint64_t expect_term = 0;
	};

	template<typename T>
	std::string to_string(const T& t) {
		return "";
	};

	template<>
	std::string to_string<entry_t>(const entry_t& entry) {
		std::stringstream ss;
		ss << "{ term=" << entry.term << ", index =" << entry.index << ", type=" << entry.type << "}";
		return ss.str();
	}

	struct request_vote_t {
		/** currentTerm, to force other leader/candidate to step down */
		uint64_t term;

		/** candidate requesting vote */
		int from;

		/** index of candidate's last log entry */
		uint64_t last_log_idx;

		/** term of candidate's last log entry */
		uint64_t last_log_term;

		MSGPACK_DEFINE(term, from, last_log_idx, last_log_term)
	};

	struct response_vote {
		/** currentTerm, for candidate to update itself */
		uint64_t term = 0;

		/** true means candidate received vote */
		bool vote_granted = false;

		MSGPACK_DEFINE(term, vote_granted)
	};

	struct req_append_entry {
		int from;
		uint64_t term;
		uint64_t prev_log_index;
		uint64_t prev_log_term;
		uint64_t leader_commit_index;
		std::vector<entry_t> entries;
		MSGPACK_DEFINE(from, term, prev_log_index, prev_log_term, leader_commit_index, entries)
	};

	struct res_append_entry {
		int from = 0;
		uint64_t term = 0;
		bool reject = false;
		uint64_t last_log_index = 0;
		uint64_t reject_hint =0 ; //the position of log rejecting
		MSGPACK_DEFINE(from, term, reject, last_log_index, reject_hint);
	};

	struct req_heartbeat {
		int from;
		uint64_t term;
		uint64_t leader_commit_index;
		MSGPACK_DEFINE(from, term, leader_commit_index);
	};

	struct res_heartbeat {
		int from;
		uint64_t term;
		MSGPACK_DEFINE(from, term);
	};

	enum entry_type {
		entry_type_data= 1,
		entry_type_none= 2,
	};

	struct ask_leader_req{};
	struct res_ask_leader{
		bool is_leader = false;
		int64_t leader_id = -1;
		MSGPACK_DEFINE(is_leader, leader_id);
	};

}