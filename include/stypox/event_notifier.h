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

public:
	friend class Handler;
	class Handler {
		friend class EventNotifier;

		struct M_Position {
			std::variant<size_t, std::pair<size_t, size_t>> hash;
			M_EventFunctionBase* pointer;
		};

		EventNotifier* const m_eventNotifier;
		std::vector<M_Position> m_positions;
		
		Handler(EventNotifier* eventNotifier) :
			m_eventNotifier{eventNotifier} {}
	public:
		~Handler() {
			disconnect();
		}
		void keep() { // WARNING use this only when the connected functions do not own resources that are destructed before the main EventNotifier is
			m_positions.resize(0);
		}
		void disconnect() {
			functions_t* functions;
			for (auto&& position : m_positions) {
				if(std::holds_alternative<size_t>(position.hash)) {
					auto& hash = std::get<size_t>(position.hash);
					functions = &m_eventNotifier->m_type_functions[hash];
				}
				else {
					auto& hash = std::get<std::pair<size_t, size_t>>(position.hash);
					functions = &m_eventNotifier->m_value_functions[hash.first][hash.second];
				}

				functions->erase(std::find_if(functions->begin(), functions->end(),
					[position](const std::unique_ptr<M_EventFunctionBase>& p){
						return p.get() == position.pointer;
					}));
			}

			m_positions.resize(0);
		}
	};

private:
	template<class F, class E>
	static constexpr bool is_event_function_valid = F::arg_count == 0 || (F::arg_count == 1 && std::is_same_v<E, typename F::argument_type>);

	using functions_t = std::vector<std::unique_ptr<M_EventFunctionBase>>;
	using hash_to_functions_t = std::map<size_t, functions_t>;

	// functions to be notified when the event type corresponds
	std::map<size_t, functions_t> m_type_functions;
	// functions to be notified only when both the type and the value of the event correspond
	std::map<size_t, hash_to_functions_t> m_value_functions;

	template<class E, class F>
	void add_and_check(F* event_function, functions_t& where) {
		where.push_back(std::unique_ptr<M_EventFunctionBase>{event_function});

		static_assert(is_event_function_valid<F, E>,
			"function takes either no argument or exaclty one of the same type of the event");
	}
	
	template<class F, size_t I = 0, class... Events>
	void connect_impl(Handler& handler, F function, Events... events) {
		auto event = std::get<I>(std::tuple<Events...>(events...));
		using event_type = decltype(event);
		size_t type_hash = typeid(event_type).hash_code(),
			value_hash = std::hash<event_type>{}(event);

		auto event_function = new M_EventFunction{std::function{function}};
		add_and_check<event_type>(event_function, m_value_functions[type_hash][value_hash]);

		handler.m_positions.push_back({std::pair{type_hash, value_hash}, event_function});
		if constexpr(I+1 != sizeof...(Events))
			connect_impl<F, I+1>(handler, function, events...);
	}

	template<class T, class R, size_t I = 0, class... Args, class... Events>
	void connect_member_impl(Handler& handler, T& object, R(T::*member_function)(Args...), Events... events) {
		auto event = std::get<I>(std::tuple<Events...>(events...));
		using event_type = decltype(event);
		size_t type_hash = typeid(event_type).hash_code(),
			value_hash = std::hash<event_type>{}(event);
		
		auto event_function = new M_EventMemberFunction{object, member_function};
		add_and_check<event_type>(event_function, m_value_functions[type_hash][value_hash]);

		handler.m_positions.push_back({std::pair{type_hash, value_hash}, event_function});
		if constexpr(I+1 != sizeof...(Events))
			connect_member_impl<T, R, I+1>(handler, object, member_function, events...);
	}
public:
	// add [function] to be called when an event of type [E] happens
	template<class E, class F>
	[[nodiscard]] Handler connect(F function) {
		size_t type_hash = typeid(E).hash_code();

		auto event_function = new M_EventFunction{std::function{function}};
		add_and_check<E>(event_function, m_type_functions[type_hash]);
		
		Handler handler{this};
		handler.m_positions.push_back({type_hash, event_function});
		return handler;
	}
	// add [function] for every i, to be called when an event of type [Events](at index i) with the same std::hash as [events](at index i) happens
	template<class F, size_t I = 0, class... Events>
	[[nodiscard]] Handler connect(F function, Events... events) {
		Handler handler{this};
		connect_impl(handler, function, events...);
		return handler;
	}

	// add [member_function] to be called on [object] when an event of type [E] happens
	template<class E, class T, class R, class... Args>
	[[nodiscard]] Handler connect_member(T& object, R(T::*member_function)(Args...)) {
		size_t type_hash = typeid(E).hash_code();

		auto event_function = new M_EventMemberFunction{object, member_function};
		add_and_check<E>(event_function, m_type_functions[type_hash]);

		Handler handler{this};
		handler.m_positions.push_back({type_hash, event_function});
		return handler;
	}
	// add [member_function] for every i, to be called on [object] when an event of type [Events](at index i) with the same std::hash as [events](at index i) happens
	template<class T, class R, class... Args, class... Events>
	[[nodiscard]] Handler connect_member(T& object, R(T::*member_function)(Args...), Events... events) {
		Handler handler{this};
		connect_member_impl(handler, object, member_function, events...);
		return handler;
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

} // namespace stypox

#endif // _STYPOX_EVENT_NOTIFIER_H_