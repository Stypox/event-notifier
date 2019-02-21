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

		using functions_t = std::vector<std::unique_ptr<M_EventFunctionBase>>;
		using hash_to_functions_t = std::map<size_t, functions_t>;

		// functions to be notified when the event type corresponds
		std::map<size_t, functions_t> m_typeFunctions;
		// functions to be notified only when both the type and the value of the event correspond
		std::map<size_t, hash_to_functions_t> m_valueFunctions;
	public:
		// add [function] to be called when an event of type [E] happens
		template<class E, class F>
		void connect(F function) {
			auto eventFunction = new M_EventFunction{std::function{function}};

			// check that the only [function] argument is the same type as [event]
			using argument_type = typename std::remove_pointer_t<decltype(eventFunction)>::argument_type;
			static_assert(std::is_same_v<E, argument_type>,
				"The function passed to stypox::EventNotifier::connect must take one argument of the same type of event.");

			size_t typeHash = typeid(E).hash_code();

			// add function to map
			m_typeFunctions[typeHash]
				.push_back(std::unique_ptr<M_EventFunctionBase>{eventFunction});
		}
		// add [function] to be called when an event of type [E] with the same std::hash as [event] happens
		template<class E, class F>
		void connect(E event, F function) {
			auto eventFunction = new M_EventFunction{std::function{function}};

			// check that the only @function argument is the same type as @event
			using argument_type = typename std::remove_pointer_t<decltype(eventFunction)>::argument_type;
			static_assert(std::is_same_v<E, argument_type>,
				"The function passed to stypox::EventNotifier::connect must take one argument of the same type of event.");

			size_t typeHash = typeid(event).hash_code();
			size_t eventHash = std::hash<E>{}(event);

			// add function to map
			m_valueFunctions[typeHash][eventHash]
				.push_back(std::unique_ptr<M_EventFunctionBase>{eventFunction});
		}

		// add [memberFunction] to be called on [object] when an event of type [E] happens
		template<class E, class T, class F>
		void connectMember(T& object, F memberFunction) {
			connect<E>([&object, memberFunction](E event){
				(object.*memberFunction)(event);
			});
		}
		// add [memberFunction] to be called on [object] when an event of type [E] with the same std::hash as [event] happens
		template<class E, class T, class F>
		void connectMember(E event, T& object, F memberFunction) {
			connect(event, [&object, memberFunction](E event){
				(object.*memberFunction)(event);
			});
		}

		template<class E>
		void notify(E event) {
			size_t typeHash = typeid(event).hash_code();

			for(auto&& function : m_typeFunctions[typeHash])
				function->call(reinterpret_cast<void*>(&event));
			if constexpr(is_hashable_v<E>) {
				for(auto&& function : m_valueFunctions[typeHash][std::hash<E>{}(event)])
					function->call(reinterpret_cast<void*>(&event));
			}
		}
	};
}

#endif