CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra
LDFLAGS ?=

SRC_DIR := src
INC_DIR := include
BIN_DIR := bin

SRC := $(wildcard $(SRC_DIR)/*.cpp)
TARGETS := $(patsubst $(SRC_DIR)/%.cpp,$(BIN_DIR)/%,$(SRC))

.PHONY: all clean list enterprise

all: $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) $< -o $@ $(LDFLAGS)

enterprise:
	./enterprise_project/build.sh all

list:
	@echo "sources:"; for f in $(SRC); do echo "  - $$f"; done
	@echo "binaries:"; for t in $(TARGETS); do echo "  - $$t"; done

clean:
	rm -rf $(BIN_DIR)
	$(MAKE) -C enterprise_project clean || true
