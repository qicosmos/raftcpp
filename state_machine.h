#pragma once
#include <thread>
#include "entity.h"

namespace raftcpp {
	class state_machine_t {
	public:
		static state_machine_t& get() {
			static state_machine_t instance;
			return instance;
		}

		
		bool apply(const entry_t& e) {
			//TODO

			return true;
		}

	private:

		state_machine_t() = default;
		state_machine_t(const state_machine_t&) = delete;
		state_machine_t(state_machine_t&&) = delete;
		
	};
}