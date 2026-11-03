CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -pedantic -O2

.PHONY: test
test:
	@mkdir -p /tmp/palpulse-tests
	$(CXX) $(CXXFLAGS) tests/test_wire.cpp -o /tmp/palpulse-tests/test_wire
	$(CXX) $(CXXFLAGS) tests/test_accumulator.cpp -o /tmp/palpulse-tests/test_accumulator
	/tmp/palpulse-tests/test_wire
	/tmp/palpulse-tests/test_accumulator
