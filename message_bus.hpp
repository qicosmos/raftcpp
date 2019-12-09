#pragma once
#include <string>
#include <map>
#include <functional>
#include <cassert>
#include <meta_util.hpp>

namespace raftcpp {
	enum class MessageKey;
	template<typename T>
	constexpr auto non_const_lvalue_reference_v = !std::is_const_v<std::remove_reference_t<T>> && std::is_lvalue_reference_v<T>;

	class message_bus {
	public:
		static message_bus& get() {
			static message_bus instance;
			return instance;
		}

		template<MessageKey key, typename Function, typename = std::enable_if_t<!std::is_member_function_pointer<Function>::value>>
		void subscribe(const Function & f) {
			using Tuple = typename function_traits<Function>::tuple_type;
			static_assert(!has_non_const_reference<Tuple>::value, "don't support non const lvalue reference!");
			check_duplicate<key>();
			invokers_[key] = { std::bind(&invoker<Function>::apply, f, std::placeholders::_1, std::placeholders::_2) };
		}

		template <MessageKey key, typename Function, typename Self, typename = std::enable_if_t<std::is_member_function_pointer<Function>::value>>
		void subscribe(const Function & f, Self * t) {
			using Tuple = typename function_traits<Function>::tuple_type;
			static_assert(!has_non_const_reference<Tuple>::value, "don't support non const lvalue reference!");
			check_duplicate<key>();
			invokers_[key] = { std::bind(&invoker<Function>::template apply_mem<Self>, f, t, std::placeholders::_1, std::placeholders::_2) };
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
		void send_msg(Args&& ... args) {
			auto it = invokers_.find(key);
			assert(it != invokers_.end());

			call_impl(it, nullptr, std::forward<Args>(args)...);
		}

	private:
		message_bus() {};
		message_bus(const message_bus&) = delete;
		message_bus(message_bus&&) = delete;

		template<typename T>
		struct non_const_lvalue_reference_t : public std::integral_constant<bool, non_const_lvalue_reference_v<T>> {
		};

		template <typename Tuple>
		struct has_non_const_reference;

		template <typename... Us>
		struct has_non_const_reference<std::tuple<Us...>> : std::disjunction<non_const_lvalue_reference_t<Us>...> {};

		template <typename T, typename ... Args>
		void call_impl(T it, void* ptr, Args&& ... args) {
			using Tuple = decltype(std::make_tuple(std::forward<Args>(args)...));
			using storage_type = typename std::aligned_storage<sizeof(Tuple), alignof(Tuple)>::type;
			storage_type data;
			Tuple* tp = new (&data) Tuple;
			*tp = std::forward_as_tuple(std::forward<Args>(args)...);

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
				using R = typename function_traits<Function>::return_type;
				tuple_type* tp = static_cast<tuple_type*>(bl);
				call<R>(func, *tp, result);
			}

			template<typename R, typename F, typename ... Args>
			static typename std::enable_if<std::is_void<R>::value>::type
				call(const F& f, std::tuple<Args...>& tp, void*) {
				call_helper<void>(f, std::make_index_sequence<sizeof... (Args)>{}, tp);
			}

			template<typename R, typename F, typename ... Args>
			static typename std::enable_if<!std::is_void<R>::value>::type
				call(const F& f, std::tuple<Args...>& tp, void* result) {
				R r = call_helper<R>(f, std::make_index_sequence<sizeof... (Args)>{}, tp);
				if (result)
					* (decltype(r)*)result = r;
			}

			template<typename R, typename F, size_t... I, typename ... Args>
			static auto call_helper(const F& f, const std::index_sequence<I...>&, std::tuple<Args...>& tup) {//-> typename std::result_of<F(Args...)>::type {
				using tuple_type = typename function_traits<Function>::tuple_type;
				return f(((std::tuple_element_t<I, tuple_type>)std::get<I>(tup))...);
			}

			template<typename TupleDest, size_t... I, typename TupleSrc>
			static inline TupleDest get_dest_tuple(std::index_sequence<I...>, TupleSrc& src) {
				return std::forward_as_tuple(((std::tuple_element_t<I, TupleDest>)std::get<I>(src))...);
			}

			template <typename Self>
			static inline void apply_mem(const Function& f, Self* self, void* bl, void* result) {
				using bare_tuple_type = typename function_traits<Function>::bare_tuple_type;
				bare_tuple_type* tp = static_cast<bare_tuple_type*>(bl);

				using tuple_type = typename function_traits<Function>::tuple_type;
				auto dest_tp = get_dest_tuple<tuple_type>(std::make_index_sequence<std::tuple_size<bare_tuple_type>::value>{}, * tp);

				using return_type = typename function_traits<Function>::return_type;
				call_mem<return_type>(f, self, dest_tp, result, std::integral_constant<bool, std::is_void<return_type>::value>{});
			}

			template<typename R, typename F, typename Self, typename ... Args>
			static void call_mem(const F& f, Self* self, const std::tuple<Args...>& tp, void*, std::true_type) {
				call_member_helper<void>(f, self, std::make_index_sequence<sizeof...(Args)>{}, tp);
			}

			template<typename R, typename F, typename Self, typename ... Args>
			static void call_mem(const F& f, Self* self, const std::tuple<Args...>& tp, void* result, std::false_type) {
				auto r = call_member_helper<R>(f, self, std::make_index_sequence<sizeof...(Args)>{}, tp);
				if (result)
					* (R*)result = r;
			}

			template<typename R, typename F, typename Self, size_t... I, typename ... Args>
			static R call_member_helper(const F& f, Self* self, const std::index_sequence<I...>&, const std::tuple<Args...>& tup) {//-> decltype((self->*f)(std::get<I>(tup)...)) {
				using tuple_type = typename function_traits<F>::tuple_type;
				return (self->*f)(((std::tuple_element_t<I, tuple_type>)std::get<I>(tup))...);
			}
		};

	private:
		std::map<MessageKey, std::function<void(void*, void*)>> invokers_;
	};
}