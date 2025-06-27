CC = g++
CFLAGS = -Wall -g -std=c++17 -Wextra -O2
LDFLAGS = 
SRCDIR = src

# Object files for each executable
SOLVER_OBJS = Solver.o CoupVariantBState.o
VIEWER_OBJS = Viewer.o

# Default target
all: solver viewer

# solver executable
solver: $(SOLVER_OBJS)
	$(CC) $(SOLVER_OBJS) -o solver $(LDFLAGS)

# viewer executable
viewer: $(VIEWER_OBJS)
	$(CC) $(VIEWER_OBJS) -o viewer $(LDFLAGS)

Solver.o: $(SRCDIR)/Solver.cpp $(SRCDIR)/CoupVariantBState.hpp
	$(CC) $(CFLAGS) -c $(SRCDIR)/Solver.cpp

CoupVariantBState.o: $(SRCDIR)/CoupVariantBState.cpp $(SRCDIR)/CoupVariantBState.hpp
	$(CC) $(CFLAGS) -c $(SRCDIR)/CoupVariantBState.cpp

Viewer.o: $(SRCDIR)/Viewer.cpp
	$(CC) $(CFLAGS) -c $(SRCDIR)/Viewer.cpp

# Clean up
clean:
	rm -f *.o solver viewer

.PHONY: all clean