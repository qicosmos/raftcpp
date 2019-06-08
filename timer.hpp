#pragma once
#include "common.h"
#include "message_bus.hpp"

namespace raftcpp {
	class timer_t {
	public:
		timer_t() : election_timer_(ios_), vote_timer_(ios_), heartbeat_timer_(ios_),
			work_(ios_), bus_(message_bus::get()) {
			init();
			thd_ = std::make_shared<std::thread>([this] { ios_.run(); });
		}

		~timer_t() {
			ios_.stop();
			thd_->join();
		}

	private:
		void init() {
			bus_.subscribe<msg_restart_election_timer>(&timer_t::restart_election_timer, this);
			bus_.subscribe<msg_cancel_election_timer>(&timer_t::cacel_election_timer, this);

			bus_.subscribe<msg_restart_vote_timer>(&timer_t::restart_vote_timer, this);
			bus_.subscribe<msg_cancel_vote_timer>(&timer_t::cacel_vote_timer, this);

			bus_.subscribe<msg_restart_heartbeat_timer>(&timer_t::restart_heartbeat_timer, this);
			bus_.subscribe<msg_cancel_heartbeat_timer>(&timer_t::cacel_heartbeat_timer, this);
		}

		void restart_election_timer(int timeout) {
			start_timer<msg_election_timeout>(election_timer_, timeout);
		}

		void cacel_election_timer() {
			cancel_timer(election_timer_);
		}

		void restart_vote_timer() {
			start_timer<msg_vote_timeout>(vote_timer_, VOTE_TIMEOUT);
		}

		void cacel_vote_timer() {
			cancel_timer(vote_timer_);
		}

		void restart_heartbeat_timer() {
			start_timer<msg_heartbeat_timeout>(heartbeat_timer_, HEARTBEAT_PERIOD);
		}

		void cacel_heartbeat_timer() {
			cancel_timer(heartbeat_timer_);
		}

		template<MessageKey key>
		void start_timer(boost::asio::steady_timer& timer, int timeout) {
			timer.expires_from_now(std::chrono::milliseconds(timeout));
			timer.async_wait([this](const boost::system::error_code & ec) {
				if (ec) {
					return;
				}

				bus_.send_msg<key>();
			});
		}

		void cancel_timer(boost::asio::steady_timer& timer) {
			boost::system::error_code ignore;
			timer.cancel(ignore);
		}
	private:
		asio::io_service ios_;
		boost::asio::io_service::work work_;
		boost::asio::steady_timer election_timer_;
		boost::asio::steady_timer vote_timer_;
		boost::asio::steady_timer heartbeat_timer_;

		std::shared_ptr<std::thread> thd_;
		bool stop_ = false;

		message_bus& bus_;
	};
}