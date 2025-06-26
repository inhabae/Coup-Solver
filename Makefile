CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
SRC_DIR = src
BUILD_DIR = build

# Source files
SRCS = $(SRC_DIR)/Solver.cpp $(SRC_DIR)/CoupVariantAState.cpp $(SRC_DIR)/Viewer.cpp
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Targets
TARGETS = solver viewer

all: $(TARGETS)

# Link the solver executable
solver: $(BUILD_DIR)/Solver.o $(BUILD_DIR)/CoupVariantAState.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link the viewer executable
viewer: $(BUILD_DIR)/Viewer.o $(BUILD_DIR)/CoupVariantAState.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile each .cpp to .o into build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGETS)

.PHONY: all clean