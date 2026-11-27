CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror

build/replay: src/main.cpp src/replay.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) src/main.cpp -o build/replay

test: build/replay
	python3 -m unittest discover -s tests -v

sanitize:
	mkdir -p build
	$(CXX) -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic src/main.cpp -o build/replay-sanitize
	REPLAY_BINARY=build/replay-sanitize python3 -m unittest discover -s tests -v
