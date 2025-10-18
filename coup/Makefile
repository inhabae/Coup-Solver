CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -pedantic -O2 -std=c++17

# Default target: build both executables
all: solver test

# Solver executable
solver: game_state.o trainer.o solver.o
	$(CXX) $(CXXFLAGS) -o solver game_state.o trainer.o solver.o

# Test executable
test: game_state.o trainer.o test.o
	$(CXX) $(CXXFLAGS) -o test game_state.o trainer.o test.o

# Object files
solver.o : trainer.hpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c solver.cpp

test.o: test.cpp game_state.hpp trainer.hpp
	$(CXX) $(CXXFLAGS) -c test.cpp

game_state.o: game_state.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c game_state.cpp

trainer.o: trainer.cpp trainer.hpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c trainer.cpp

clean:
	rm -f *.o trainer test
