# event-notifier
A **C++ header-only** library that lets you **connect events to functions** with a **small** and **intuitive** interface. When an event occours, all the functions connected to it are called. **Member functions** (like `game.pause()`) are supported, too!
# Connect events to functions
When connecting some events to a function, you can choose whether to **connect all events** of that type to the function **or only some** (based on `std::hash<Event>`) [``en`` is an EventNotifier]:
- `en.connect<Event>(function)`: connects every event of type `Event` to `function`. E.g. `en.connect<Click>(function)` would connect every event of type `Click` to the function.
- `en.connect(function, events...)`: connects every event in `events...` to `function`. `std::hash` is used to compare for equality. E.g. `en.connect(function, Click{right})` would connect every event that has the same hash as `Click{right}` to the function.

`function` can be any function convertible to `std::function` (normal functions and lambdas are ok). It must take either 0 arguments or 1 argument (the event).

If you need to connect to a member function, you can use `en.connect_member()` [in the examples `game` is of type `Game`]:
- `en.connect_member<Event>(object, &object_type::member_function)`: connects every event of type `Event` to `(object.*member_function)()`. E.g. `en.connect_member<Click>(game, &Game::pause)`
- `en.connect_member(object, &object_type::member_function, events...)`: connects every event in `events...` to `(object.*member_function)()`. E.g. `en.connect_member(game, &Game::pause, Click{right})`

Every connect\[_member\]() call returns a `EventNotifier::Handler`, that can be used to **disconnect the function** when it is not needed / it can not be used anymore. It uses **RAII**, so its destructor takes care of disconnecting the function it handles, even though the disconnection can be done manually using ``handler.disconnect()``. If you do not need this kind of handling (because the connected function will always be valid) you can do `handler.keep()`, that will transfer the ownership to the EventNotifier. For example: `en.connect(closeGame, WindowButton{X}).keep()`. The compiler will warn you if you do nothing with the returned Handler.

# Event notification
When you want an event to occour, you have to **pass it to the EventNotifier** through the `en.notify(event)` function.    
For example: `en.notify(Click{right})`

# Installation
To use this library just download the header file `event_notifier.h` and `#include` it in your project! If you want to `#include` it as `<stypox/event_notifier.h>` you need to add `-IPATH/TO/event-notifier/include` to your compiler options. Note: it requires C++17, so add to your compiler options `-std=c++17`.

# Example
```cpp
#include <stypox/event_notifier.h>
#include <iostream>
using stypox::EventNotifier;

enum Click { right, left };
struct MouseScroll { int deltaPixels; };

class Game {
	EventNotifier::Handler m_rightClickHandler;
	// ^ will automatically be disconnected when Game is destructed.
public:
	Game(EventNotifier& en) :
		m_rightClickHandler{en.connect_member(*this, &Game::onRightClick, Click::right)}	{}
	void onRightClick() { std::cout << "Game received a right click!\n"; }
};

int main() {
	EventNotifier en;
	en.connect<MouseScroll>([](MouseScroll ms){ std::cout << "Scrolled by " << ms.deltaPixels << " pixels\n"; }).keep();
	// ^ here `.keep()` is used since the lambda will always be available: it does not depend on any object that may be destructed before `en` is.

	en.notify(MouseScroll{17}); // prints "Scrolled by 17 pixels"

	{
		Game game{en};
		en.notify(Click::right); // prints "Game received a right click!"
		en.notify(Click::left); // does nothing
	}
	en.notify(Click::right); // does nothing since `game` has already been destructed
	en.notify(MouseScroll{23}); // prints "Scrolled by 23 pixels"

	// no leaks: when `en` is destructed all functions are disconnected.
}
```
The complete output will be:
```
Scrolled by 17 pixels
Game received a right click!
Scrolled by 23 pixels
```