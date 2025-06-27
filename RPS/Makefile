CC = g++
CFLAGS = -Wall -g -std=c++17 -Wextra -O2
LDFLAGS = 
SRCDIR = src

# Object files for each executable
RPS_SOLVER_OBJS = RPSSolver.o RPSState.o
RPS_VIEWER_OBJS = RPSViewer.o

# Default target
all: rps_solver rps_viewer

# rps_solver executable
rps_solver: $(RPS_SOLVER_OBJS)
	$(CC) $(RPS_SOLVER_OBJS) -o rps_solver $(LDFLAGS)

# rps_viewer executable
rps_viewer: $(RPS_VIEWER_OBJS)
	$(CC) $(RPS_VIEWER_OBJS) -o rps_viewer $(LDFLAGS)

# Object file rules
RPSSolver.o: $(SRCDIR)/RPSSolver.cpp $(SRCDIR)/RPSState.hpp
	$(CC) $(CFLAGS) -c $(SRCDIR)/RPSSolver.cpp

RPSViewer.o: $(SRCDIR)/RPSViewer.cpp
	$(CC) $(CFLAGS) -c $(SRCDIR)/RPSViewer.cpp

RPSState.o: $(SRCDIR)/RPSState.cpp $(SRCDIR)/RPSState.hpp
	$(CC) $(CFLAGS) -c $(SRCDIR)/RPSState.cpp

# Clean up
clean:
	rm -f *.o rps_solver solver viewer

.PHONY: all clean