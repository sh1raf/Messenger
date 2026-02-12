CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude -I../database/include
LDFLAGS := -lpqxx -lpq -lpthread -lssl -lcrypto

SERVER_BIN := messenger_server
TEST_CLIENT_BIN := test_client

SERVER_SOURCES := src/main.cpp src/server.cpp src/session.cpp
SERVER_OBJECTS := $(SERVER_SOURCES:.cpp=.o)

TEST_CLIENT_SOURCES := src/test_client.cpp src/client.cpp
TEST_CLIENT_OBJECTS := $(TEST_CLIENT_SOURCES:.cpp=.o)

.PHONY: all build run test clean

all: build

build: $(SERVER_BIN) $(TEST_CLIENT_BIN)

run: $(SERVER_BIN)
	./$(SERVER_BIN)

test: $(TEST_CLIENT_BIN)
	./$(TEST_CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJECTS)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJECTS) -o $(SERVER_BIN) $(LDFLAGS)

$(TEST_CLIENT_BIN): $(TEST_CLIENT_OBJECTS)
	$(CXX) $(CXXFLAGS) $(TEST_CLIENT_OBJECTS) -o $(TEST_CLIENT_BIN)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJECTS) $(TEST_CLIENT_OBJECTS) $(SERVER_BIN) $(TEST_CLIENT_BIN)
