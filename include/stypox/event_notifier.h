#ifndef _STYPOX_EVENT_NOTIFIER_H_
#define _STYPOX_EVENT_NOTIFIER_H_
#include <functional>
#include <map>
#include <memory>
#include <variant>

namespace stypox {
	template <typename T, typename = std::void_t<>>
	struct is_hashable : std::false_type {};
	template <typename T>
	struct is_hashable<T, std::void_t<decltype(std::declval<std::hash<T>>()(std::declval<T>()))>> : std::true_type {};
	template <typename T>
	constexpr bool is_hashable_v = is_hashable<T>::value;

	class EventNotifier {
		class M_EventFunctionBase {
		public:
			virtual ~M_EventFunctionBase() {};
			virtual void call(void* data) = 0;
		};

		template<class R, class... Args>
		class M_EventFunction : public M_EventFunctionBase {
			std::function<R(Args...)> m_function;
		public:
			static constexpr size_t arg_count = sizeof...(Args);
			//chooses void only when arg_count == 0
			using argument_type = std::tuple_element_t<0, std::tuple<Args..., void>>;

			~M_EventFunction() override {};
			M_EventFunction(std::function<R(Args...)> function) :
				m_function{function} {}
			void call(void* data) override {
				if constexpr(arg_count == 0)
					m_function();
				else
					m_function(*static_cast<argument_type*>(data));
			}
		};

		template<class T, class R, class... Args>
		class M_EventMemberFunction : public M_EventFunctionBase {
			T& m_object;
			R(T::*m_function)(Args...);
		public:
			static constexpr size_t arg_count = sizeof...(Args);
			//chooses void only when arg_count == 0
			using argument_type = std::tuple_element_t<0, std::tuple<Args..., void>>;

			~M_EventMemberFunction() override {};
			M_EventMemberFunction(T& object, R(T::*function)(Args...)) :
				m_object{object}, m_function{function} {}
			void call(void* data) override {
				if constexpr(arg_count == 0)
					(m_object.*m_function)();
				else
					(m_object.*m_function)(*static_cast<argument_type*>(data));
			}
		};

		template<class F, class E>
		static constexpr bool is_event_function_valid = F::arg_count == 0 || (F::arg_count == 1 && std::is_same_v<E, typename F::argument_type>);

		using functions_t = std::vector<std::unique_ptr<M_EventFunctionBase>>;
		using hash_to_functions_t = std::map<size_t, functions_t>;

		// functions to be notified when the event type corresponds
		std::map<size_t, functions_t> m_type_functions;
		// functions to be notified only when both the type and the value of the event correspond
		std::map<size_t, hash_to_functions_t> m_value_functions;
	public:
		// add [function] to be called when an event of type [E] happens
		template<class E, class F>
		void connect(F function) {
			// add function to map
			auto event_function = new M_EventFunction{std::function{function}};
			m_type_functions[typeid(E).hash_code()]
				.push_back(std::unique_ptr<M_EventFunctionBase>{event_function});

			static_assert(is_event_function_valid<typename std::remove_pointer_t<decltype(event_function)>, E>,
				"function takes either no argument or exaclty one of the same type of the event");
		}
		// add [function] to be called when an event of type [E] with the same std::hash as [event] happens
		template<class E, class F>
		void connect(E event, F function) {
			// add function to map
			auto event_function = new M_EventFunction{std::function{function}};
			m_value_functions[typeid(event).hash_code()][std::hash<E>{}(event)]
				.push_back(std::unique_ptr<M_EventFunctionBase>{event_function});

			static_assert(is_event_function_valid<typename std::remove_pointer_t<decltype(event_function)>, E>,
				"function takes either no argument or exaclty one of the same type of the event");
		}

		// add [member_function] to be called on [object] when an event of type [E] happens
		template<class E, class T, class R, class... Args>
		void connect_member(T& object, R(T::*member_function)(Args...)) {
			// add function to map
			auto event_function = new M_EventMemberFunction{object, member_function};
			m_type_functions[typeid(E).hash_code()]
				.push_back(std::unique_ptr<M_EventFunctionBase>{event_function});

			static_assert(is_event_function_valid<typename std::remove_pointer_t<decltype(event_function)>, E>,
				"function takes either no argument or exaclty one of the same type of the event");
		}
		// add [member_function] to be called on [object] when an event of type [E] with the same std::hash as [event] happens
		template<class E, class T, class R, class... Args>
		void connect_member(E event, T& object, R(T::*member_function)(Args...)) {
			// add function to map
			auto event_function = new M_EventMemberFunction{object, member_function};
			m_value_functions[typeid(event).hash_code()][std::hash<E>{}(event)]
				.push_back(std::unique_ptr<M_EventFunctionBase>{event_function});

			static_assert(is_event_function_valid<typename std::remove_pointer_t<decltype(event_function)>, E>,
				"function takes either no argument or exaclty one of the same type of the event");
		}

		template<class E>
		void notify(E event) {
			size_t type_hash = typeid(event).hash_code();

			for(auto&& function : m_type_functions[type_hash])
				function->call(reinterpret_cast<void*>(&event));
			if constexpr(is_hashable_v<E>) {
				for(auto&& function : m_value_functions[type_hash][std::hash<E>{}(event)])
					function->call(reinterpret_cast<void*>(&event));
			}
		}
	};
}

#endif