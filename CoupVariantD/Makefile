CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -pedantic -O2 -std=c++17

solver: game_state.o solver.o
	$(CXX) $(CXXFLAGS) -o solver game_state.o solver.o

test: game_state.o test.o
	$(CXX) $(CXXFLAGS) -o test game_state.o test.o

test.o: test.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c test.cpp


game_state.o: game_state.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c game_state.cpp

solver.o: solver.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c solver.cpp

clean:
	rm -f *.o solver test
