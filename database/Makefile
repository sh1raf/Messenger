CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude
LDFLAGS := -lpqxx -lpq

TEST_BIN := db_tests

.PHONY: all test run clean

all: $(TEST_BIN)

test: $(TEST_BIN)

run: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): tests/db_tests.cpp include/postgresql.hpp
	$(CXX) $(CXXFLAGS) tests/db_tests.cpp -o $(TEST_BIN) $(LDFLAGS)

clean:
	rm -f $(TEST_BIN)
