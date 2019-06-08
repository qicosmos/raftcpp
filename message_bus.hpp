#pragma once
#include <string>
#include <map>
#include <functional>
#include <cassert>
#include <meta_util.hpp>

namespace raftcpp {
	enum class MessageKey;

	class message_bus {
	public:
		static message_bus& get() {
			static message_bus instance;
			return instance;
		}

		template<MessageKey key, typename Function, typename = std::enable_if_t<!std::is_member_function_pointer<Function>::value>>
		void subscribe(const Function & f) {
			using namespace std::placeholders;
			check_duplicate<key>();
			invokers_[key] = { std::bind(&invoker<Function>::apply, f, _1, _2) };
		}

		template <MessageKey key, typename Function, typename Self, typename = std::enable_if_t<std::is_member_function_pointer<Function>::value>>
		void subscribe(const Function& f, Self * t) {
			using namespace std::placeholders;
			check_duplicate<key>();
			invokers_[key] = { std::bind(&invoker<Function>::template apply_mem<Self>, f, t, _1, _2) };
		}

		//non-void function
		template <MessageKey key, typename T, typename ... Args>
		T send_msg(Args&& ... args) {
			auto it = invokers_.find(key);
			assert(it != invokers_.end());

			T t;
			call_impl(it, &t, std::forward<Args>(args)...);
			return t;
		}

		//void function
		template <MessageKey key, typename ... Args>
		void send_msg(Args && ... args) {
			auto it = invokers_.find(key);
			assert(it != invokers_.end());

			call_impl(it, nullptr, std::forward<Args>(args)...);
		}

	private:
		message_bus() {};
		message_bus(const message_bus&) = delete;
		message_bus(message_bus&&) = delete;

		template <typename T, typename ... Args>
		void call_impl(T it, void* ptr, Args && ... args) {
			auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
			using Tuple = decltype(args_tuple);
			using storage_type = typename std::aligned_storage<sizeof(Tuple), alignof(Tuple)>::type;
			storage_type data;
			Tuple* tp = new (&data) Tuple;
			*tp = args_tuple;

			it->second(tp, ptr);
		}

		template <MessageKey key>
		void check_duplicate() {
			auto it = invokers_.find(key);
			if (it != invokers_.end())
				assert("duplicate register");
		}

		template<typename Function>
		struct invoker {
			static inline void apply(const Function& func, void* bl, void* result) {
				using tuple_type = typename function_traits<Function>::bare_tuple_type;
				const tuple_type* tp = static_cast<tuple_type*>(bl);
				call(func, *tp, result);
			}

			template<typename F, typename ... Args>
			static typename std::enable_if<std::is_void<typename std::result_of<F(Args...)>::type>::value>::type
				call(const F& f, const std::tuple<Args...>& tp, void*) {
				call_helper(f, std::make_index_sequence<sizeof... (Args)>{}, tp);
			}

			template<typename F, typename ... Args>
			static typename std::enable_if<!std::is_void<typename std::result_of<F(Args...)>::type>::value>::type
				call(const F & f, const std::tuple<Args...> & tp, void* result) {
				auto r = call_helper(f, std::make_index_sequence<sizeof... (Args)>{}, tp);
				if (result)
					* (decltype(r)*)result = r;
			}

			template<typename F, size_t... I, typename ... Args>
			static auto call_helper(const F & f, const std::index_sequence<I...>&, const std::tuple<Args...> & tup)-> typename std::result_of<F(Args...)>::type {
				return f(std::get<I>(tup)...);
			}

			template<typename TupleDest, size_t... I, typename TupleSrc>
			static inline TupleDest get_dest_tuple(std::index_sequence<I...>, TupleSrc & src) {
				return std::forward_as_tuple(std::get<I>(src)...);
			}

			template <typename Self>
			static inline void apply_mem(const Function& f, Self * self, void* bl, void* result) {
				using bare_tuple_type = typename function_traits<Function>::bare_tuple_type;
				bare_tuple_type* tp = static_cast<bare_tuple_type*>(bl);

				using tuple_type = typename function_traits<Function>::tuple_type;
				auto dest_tp = get_dest_tuple<tuple_type>(std::make_index_sequence<std::tuple_size<bare_tuple_type>::value>{}, * tp);

				using return_type = typename function_traits<Function>::return_type;
				call_mem(f, self, dest_tp, result, std::integral_constant<bool, std::is_void<return_type>::value>{});
			}

			template<typename F, typename Self, typename ... Args>
			static void call_mem(const F& f, Self* self, const std::tuple<Args...>& tp, void*, std::true_type) {
				call_member_helper(f, self, std::make_index_sequence<sizeof...(Args)>{}, tp);
			}

			template<typename F, typename Self, typename ... Args>
			static void call_mem(const F& f, Self* self, const std::tuple<Args...>& tp, void* result, std::false_type) {
				auto r = call_member_helper(f, self, std::make_index_sequence<sizeof...(Args)>{}, tp);
				if (result)
					* (decltype(r)*)result = r;
			}

			template<typename F, typename Self, size_t... I, typename ... Args>
			static auto call_member_helper(const F& f, Self* self, const std::index_sequence<I...>&, const std::tuple<Args...>& tup)-> decltype((self->*f)(std::get<I>(tup)...)) {
				return (self->*f)(std::get<I>(tup)...);
			}
		};

	private:
		std::map<MessageKey, std::function<void(void*, void*)>> invokers_;
	};
}