CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
LDFLAGS  := -lws2_32 -lpthread

SRC_COMMON := tuplespace.cpp
SRC_SERVER := main.cpp tcp_server.cpp
SRC_TESTS  := tests.cpp

BIN_SERVER := linda_server.exe
BIN_TESTS  := linda_tests.exe

.PHONY: all server tests clean

all: server tests

server: $(SRC_SERVER) $(SRC_COMMON) main.hpp tcp_server.hpp
	$(CXX) $(CXXFLAGS) $(SRC_SERVER) $(SRC_COMMON) -o $(BIN_SERVER) $(LDFLAGS)

tests: $(SRC_TESTS) $(SRC_COMMON) main.hpp
	$(CXX) $(CXXFLAGS) $(SRC_TESTS) $(SRC_COMMON) -o $(BIN_TESTS) $(LDFLAGS)

clean:
	del /Q $(BIN_SERVER) $(BIN_TESTS) 2>nul || rm -f $(BIN_SERVER) $(BIN_TESTS)