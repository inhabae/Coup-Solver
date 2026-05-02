CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -pedantic -O2 -std=c++17

.PHONY: all clean

all: test play solver action_histories small_coup_test small_coup_solver

test: game_state.o observation.o trainer.o test.o
	$(CXX) $(CXXFLAGS) -o test game_state.o observation.o trainer.o test.o

play: game_state.o observation.o play.o
	$(CXX) $(CXXFLAGS) -o play game_state.o observation.o play.o

solver: game_state.o observation.o trainer.o solver.o
	$(CXX) $(CXXFLAGS) -o solver game_state.o observation.o trainer.o solver.o

action_histories: game_state.o observation.o action_histories.o
	$(CXX) $(CXXFLAGS) -o action_histories game_state.o observation.o action_histories.o

small_coup_test: small_coup.o small_coup_test.o
	$(CXX) $(CXXFLAGS) -o small_coup_test small_coup.o small_coup_test.o

small_coup_solver: small_coup.o small_coup_solver.o
	$(CXX) $(CXXFLAGS) -o small_coup_solver small_coup.o small_coup_solver.o

game_state.o: game_state.cpp game_state.hpp observation.hpp
	$(CXX) $(CXXFLAGS) -c game_state.cpp

observation.o: observation.cpp observation.hpp game_state.hpp
	$(CXX) $(CXXFLAGS) -c observation.cpp

test.o: test.cpp game_state.hpp observation.hpp
	$(CXX) $(CXXFLAGS) -c test.cpp

play.o: play.cpp game_state.hpp observation.hpp
	$(CXX) $(CXXFLAGS) -c play.cpp

trainer.o: trainer.cpp trainer.hpp game_state.hpp observation.hpp
	$(CXX) $(CXXFLAGS) -c trainer.cpp

solver.o: solver.cpp trainer.hpp game_state.hpp observation.hpp
	$(CXX) $(CXXFLAGS) -c solver.cpp

action_histories.o: action_histories.cpp game_state.hpp observation.hpp
	$(CXX) $(CXXFLAGS) -c action_histories.cpp

small_coup.o: small_coup.cpp small_coup.hpp
	$(CXX) $(CXXFLAGS) -c small_coup.cpp

small_coup_test.o: small_coup_test.cpp small_coup.hpp
	$(CXX) $(CXXFLAGS) -c small_coup_test.cpp

small_coup_solver.o: small_coup_solver.cpp small_coup.hpp
	$(CXX) $(CXXFLAGS) -c small_coup_solver.cpp

clean:
	rm -f *.o test play solver action_histories small_coup_test small_coup_solver
