SRC = src/
EVENT_NOTIFIER_INC = ../include
CATCH_2_INC = libs/Catch2/single_include
EXECUTABLE = test.exe

CXX = g++
CXXFLAGS = -Wall -std=c++17 -O0 -I$(EVENT_NOTIFIER_INC) -I$(CATCH_2_INC)

test: $(EXECUTABLE)
	./$(EXECUTABLE)

$(EXECUTABLE): $(SRC)test.cpp
	$(CXX) $(CXXFLAGS) -o $(EXECUTABLE) $(SRC)test.cpp

clean:
	rm $(EXECUTABLE)