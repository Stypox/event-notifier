#ifndef _STYPOX_EVENT_NOTIFIER_H_
#define _STYPOX_EVENT_NOTIFIER_H_
#include <functional>
#include <map>
#include <memory>
#include <variant>

namespace stypox {
	class M_EventFunctionBase {
	public:
		virtual ~M_EventFunctionBase() {};
		virtual void call(void* data) = 0;
	};

	template<class R, class A>
	class M_EventFunction : public M_EventFunctionBase {
	public:
		using argument_type = A;
		std::function<R(argument_type)> m_function;

		~M_EventFunction() override {};
		M_EventFunction(std::function<R(argument_type)> function) :
			m_function{function} {}
		void call(void* data) override {
			m_function(*static_cast<argument_type*>(data));
		}
	};

	class EventNotifier {
		using functions_t = std::vector<std::unique_ptr<M_EventFunctionBase>>;
		using hash_to_functions_t = std::map<size_t, functions_t>;
		std::map<size_t, std::variant<functions_t, hash_to_functions_t>> m_functions;
	public:
		template<class T, class F>
		void connect(T event, F function) {
			auto eventFunction = new M_EventFunction{std::function{function}};
			
			// check that the only @function argument is the same type as @event
			using argument_type = typename std::remove_pointer_t<decltype(eventFunction)>::argument_type;
			static_assert(std::is_same_v<T, argument_type>,
				"The function passed to stypox::EventNotifier::connect must take one argument of the same type of event.");	

			size_t type_hash = typeid(event).hash_code();
			size_t event_hash = std::hash<T>{}(event);

			// add function to map
			if (!m_functions.count(type_hash))
				m_functions.insert({type_hash, hash_to_functions_t{}});
			std::get<hash_to_functions_t>(m_functions[type_hash])[event_hash]
				.push_back(std::unique_ptr<M_EventFunctionBase>{eventFunction});
		}
		template<class T, class F>
		void connect(F function) {
			auto eventFunction = new M_EventFunction{std::function{function}};

			// check that the only @function argument is the same type as @event
			using argument_type = typename std::remove_pointer_t<decltype(eventFunction)>::argument_type;
			static_assert(std::is_same_v<T, argument_type>,
				"The function passed to stypox::EventNotifier::connect must take one argument of the same type of event.");
			
			size_t type_hash = typeid(T).hash_code();

			// add function to map
			if (!m_functions.count(type_hash))
				m_functions.insert({type_hash, functions_t{}});
			std::get<functions_t>(m_functions[type_hash])
				.push_back(std::unique_ptr<M_EventFunctionBase>{eventFunction});
		}

		template<class T>
		void notify(T event) {
			auto& functions = m_functions[typeid(event).hash_code()];
			if (std::holds_alternative<functions_t>(functions)) {
				for(auto&& function : std::get<functions_t>(functions))
					function->call(reinterpret_cast<void*>(&event));
			}
			else {
				for(auto&& function : std::get<hash_to_functions_t>(functions)[std::hash<T>{}(event)])
					function->call(reinterpret_cast<void*>(&event));
			}
		}
	};
}

#endif