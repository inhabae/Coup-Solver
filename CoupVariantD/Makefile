CXX = g++
CXXFLAGS = -Wall -O2 -std=c++14

test: game_state.o test.o
	$(CXX) $(CXXFLAGS) -o test game_state.o test.o

test.o: test.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c test.cpp

solver: game_state.o solver.o
	$(CXX) $(CXXFLAGS) -o solver game_state.o solver.o

game_state.o: game_state.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c game_state.cpp

solver.o: solver.cpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c solver.cpp

clean:
	rm -f *.o solver test
