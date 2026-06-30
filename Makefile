# Makefile for the CubeSat Telemetry Link Simulator.
#
#   make          build both the simulator and the test binary
#   make run      build and launch the live dashboard
#   make test     build and run the test suite
#   make snapshot build and render one static dashboard frame
#   make clean    remove all build artifacts
#
# Only a C++17 compiler and the standard library are required -- no external
# dependencies. -MMD -MP makes the compiler emit header dependency files so a
# changed header triggers the right rebuilds.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -MMD -MP

# All library sources (everything except the two entry points).
LIB_SRC  := src/serialization.cpp src/crc16.cpp src/framer.cpp \
            src/channel.cpp src/decoder.cpp src/satellite.cpp src/dashboard.cpp
LIB_OBJ  := $(LIB_SRC:.cpp=.o)

APP_OBJ  := src/main.o
TEST_OBJ := tests/test_main.o

BIN       := satlink
TEST_BIN  := satlink_tests

.PHONY: all run test snapshot clean
all: $(BIN) $(TEST_BIN)

$(BIN): $(LIB_OBJ) $(APP_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TEST_BIN): $(LIB_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

snapshot: $(BIN)
	./$(BIN) --once

clean:
	rm -f $(LIB_OBJ) $(APP_OBJ) $(TEST_OBJ) \
	      $(LIB_OBJ:.o=.d) $(APP_OBJ:.o=.d) $(TEST_OBJ:.o=.d) \
	      $(BIN) $(TEST_BIN)

# Pull in auto-generated header dependencies (ignored if absent).
-include $(LIB_OBJ:.o=.d) $(APP_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
