#ifndef _STYPOX_EVENT_NOTIFIER_H_
#define _STYPOX_EVENT_NOTIFIER_H_
#include <functional>
#include <map>
#include <memory>

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
		void call(void* data) override
			{ m_function(*static_cast<argument_type*>(data)); }
	};

	class EventNotifier {
		std::map<size_t, std::map<size_t, std::vector<std::unique_ptr<M_EventFunctionBase>>>> m_functions;
	public:
		template<class T, class F>
		void connect(T event, F function) {
			auto eventFunction = new M_EventFunction{std::function{function}};
			m_functions[typeid(event).hash_code()][std::hash<T>{}(event)].push_back(std::unique_ptr<M_EventFunctionBase>(eventFunction));

			using argument_type = typename std::remove_pointer_t<decltype(eventFunction)>::argument_type;
			static_assert(std::is_same_v<T, argument_type>,
				"The function passed to stypox::EventNotifier::connect must take one argument of the same type of event.");			
		}

		template<class T>
		void notify(T event) {
			for(auto&& function : m_functions[typeid(event).hash_code()][std::hash<T>{}(event)])
				function->call(reinterpret_cast<void*>(&event));
		}
	};
}

#endif