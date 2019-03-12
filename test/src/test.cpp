#include <stypox/event_notifier.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
using namespace stypox;
using Handler = EventNotifier::Handler;

enum class Event { a, b, c, d, e, f, g };

TEST_CASE("connect an event to a function", "[EventNotifier::connect()]") {
	int counter1 = 0, counter2 = 0, counter3 = 0, counter4 = 0;
	EventNotifier en;
	Handler 
		handler1 = en.connect<Event>([&counter1](){ ++counter1; }),
		handler2 = en.connect(       [&counter2](){ ++counter2; }, Event::a),
		handler3 = en.connect(       [&counter3](){ ++counter3; }, Event::b),
		handler4 = en.connect<int>(  [&counter4](int val){ counter4 += val + 1; });
	
	SECTION("functions connected by event value are only called when that exact event happens") {
		en.notify(Event::b);
		en.notify(0);
		REQUIRE(counter2 == 0);

		en.notify(Event::a);
		REQUIRE(counter2 == 1);
	}

	SECTION("functions connected by event type are only called for every instance of that type") {
		en.notify(0);
		REQUIRE(counter1 == 0);

		en.notify(Event::a);
		en.notify(Event::b);
		en.notify(Event::c);
		en.notify(Event::d);
		en.notify(Event::e);
		en.notify(Event::f);
		en.notify(Event::g);
		REQUIRE(counter1 == 7);
	}

	SECTION("every function connected to that event is called, others are not") {
		en.notify(Event::a);
		REQUIRE(counter1 == 1);
		REQUIRE(counter2 == 1);
		REQUIRE(counter3 == 0);
		REQUIRE(counter4 == 0);
	}

	SECTION("the connection still exists after a notification") {
		en.notify(Event::a);
		en.notify(Event::a);
		REQUIRE(counter1 == 2);
		REQUIRE(counter2 == 2);
	}

	SECTION("the right values are passed to the function connected by value") {
		en.notify(197);
		REQUIRE(counter4 == 198);
	}
}

TEST_CASE("connect a function to multiple events at once", "[EventNotifier::connect()]") {
	int counter = 0;
	EventNotifier en;
	Handler handler = en.connect([&counter](){ ++counter; }, Event::a, Event::b, Event::c);

	SECTION("the function is called exactly once for every connected event") {
		en.notify(Event::a);
		REQUIRE(counter == 1);
		en.notify(Event::b);
		REQUIRE(counter == 2);
		en.notify(Event::c);
		REQUIRE(counter == 3);
	}

	SECTION("after disconnection no function is left behind") {
		handler.disconnect();

		en.notify(Event::a);
		en.notify(Event::b);
		en.notify(Event::c);
		REQUIRE(counter == 0);
	}
}

TEST_CASE("connect a function to multiple equal events at once", "[EventNotifier::connect()]") {
	int counter = 0;
	EventNotifier en;
	Handler handler = en.connect([&counter](){ ++counter; }, Event::a, Event::a);

	SECTION("the function is called twice") {
		en.notify(Event::a);
		REQUIRE(counter == 2);
	}

	SECTION("the connection still exists after a notification") {
		en.notify(Event::a);
		en.notify(Event::a);
		REQUIRE(counter == 4);
	}

	SECTION("after disconnection no function is left behind") {
		handler.disconnect();

		en.notify(Event::a);
		REQUIRE(counter == 0);
	}
}

TEST_CASE("connect an event to a member function", "[EventNotifier::conect_member()]") {
	struct Type {
		int counter1 = 0, counter2 = 0;
		void member1(int value) { counter1 += value + 1; }
		void member2() { ++counter2; }
	};

	EventNotifier en;
	Type object;
	Handler
		handler1 = en.connect_member<int>(object, &Type::member1),
		handler2 = en.connect_member     (object, &Type::member2, Event::a);
	
	SECTION("every function connected to that event is called, others are not") {
		en.notify(Event::a);
		REQUIRE(object.counter1 == 0);
		REQUIRE(object.counter2 == 1);
	}

	SECTION("the connection still exists after a notification") {
		en.notify(Event::a);
		en.notify(Event::a);
		REQUIRE(object.counter1 == 0);
		REQUIRE(object.counter2 == 2);
	}

	SECTION("the right values are passed to the function connected by value") {
		en.notify(197);
		REQUIRE(object.counter1 == 198);
	}
}

TEST_CASE("disconnect an event using the handler", "[EventNotifier::Handler::disconnect()]") {
	int counter1 = 0, counter2 = 0, counter3 = 0, counter4 = 0;
	EventNotifier en;
	Handler
		handler1 = en.connect<Event>([&counter1](){ ++counter1; }),
		handler2 = en.connect<Event>([&counter2](){ ++counter2; }),
		handler3 = en.connect(       [&counter3](){ ++counter3; }, Event::a),
		handler4 = en.connect(       [&counter4](){ ++counter4; }, Event::b);

	SECTION("after disconnection the function is not left behind") {
		handler1.disconnect();
		handler2.disconnect();
		en.notify(Event::c);
		REQUIRE(counter1 == 0);
		REQUIRE(counter2 == 0);

		handler3.disconnect();
		en.notify(Event::a);
		REQUIRE(counter3 == 0);

		handler4.disconnect();
		en.notify(Event::b);
		REQUIRE(counter4 == 0);
	}

	SECTION("disconnecting one function does not influence the others") {
		handler2.disconnect();

		en.notify(Event::b);
		REQUIRE(counter1 == 1);
		REQUIRE(counter3 == 0);
		REQUIRE(counter4 == 1);
	}

	SECTION("disconnect() does not throw if the handler is already disconnected") {
		handler1.disconnect();
		REQUIRE_NOTHROW(handler1.disconnect());
	}
}

TEST_CASE("keep the function without the handler", "[EventNotifier::Handler::keep()]") {
	int counter = 0;
	EventNotifier en;
	Handler handler = en.connect([&counter](){ ++counter; }, Event::a);

	SECTION("the function is not disconnected") {
		handler.keep();
		en.notify(Event::a);
		REQUIRE(counter == 1);
	}

	SECTION("disconnect() does not throw and does not disconnect") {
		handler.keep();
		REQUIRE_NOTHROW(handler.disconnect());
		en.notify(Event::a);
		REQUIRE(counter == 1);
	}

	SECTION("calling keep() twice does not throw") {
		handler.keep();
		REQUIRE_NOTHROW(handler.keep());
	}

	SECTION("calling keep() after disconnect() does not throw and the function remains disconnected") {
		handler.disconnect();
		handler.keep();
		REQUIRE_NOTHROW(handler.keep());
		en.notify(Event::a);
		REQUIRE(counter == 0);
	}
}

TEST_CASE("destruct the handler", "[EventNotifier::Handler::~Handler()]") {
	int counter = 0;
	EventNotifier en;
	Handler* handler = new Handler{en.connect([&counter](){ ++counter; }, Event::a)};

	SECTION("disconnects the function") {
		handler->~Handler();
		en.notify(Event::a);
		REQUIRE(counter == 0);
	}

	SECTION("does not throw if the handler is already disconnected") {
		handler->disconnect();
		REQUIRE_NOTHROW(handler->~Handler());
	}
}

TEST_CASE("move the handler", "[EventNotifier::Handler()]") {
	int counter = 0;
	EventNotifier en;
	Handler handlerOld = en.connect([&counter](){ ++counter; }, Event::a);

	SECTION("does not disconnect the function") {
		Handler handlerNew{std::move(handlerOld)};
		en.notify(Event::a);
		REQUIRE(counter == 1);
	}

	SECTION("disconnection still works") {
		Handler handlerNew{std::move(handlerOld)};
		handlerNew.disconnect();
		en.notify(Event::a);
		REQUIRE(counter == 0);
	}

	SECTION("calling disconnect on the moved-from handler does not disconnect and does not throw") {
		Handler handlerNew{std::move(handlerOld)};
		REQUIRE_NOTHROW(handlerOld.disconnect());
		en.notify(Event::a);
		REQUIRE(counter == 1);
	}
}
