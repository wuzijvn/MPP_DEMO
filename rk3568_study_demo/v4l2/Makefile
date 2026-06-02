CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra
LDFLAGS ?=

SRC := $(wildcard *.cpp)
BIN_DIR := bin
TARGETS := $(patsubst %.cpp,$(BIN_DIR)/%,$(SRC))

APP ?= v4l2_capture_one_frame
DEV ?= /dev/video10
W ?= 640
H ?= 480
STAMP := $(shell date -u +%Y%m%dT%H%M%SZ)
OUT ?= ../artifacts/v4l2_capture_$(STAMP).yuyv
OUT_JPG := $(OUT:.yuyv=.jpg)

.PHONY: all one run jpg list clean

all: $(TARGETS)

one: $(BIN_DIR)/$(APP)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%: %.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

run: one
	mkdir -p "$(dir $(OUT))"
	./$(BIN_DIR)/$(APP) "$(DEV)" "$(W)" "$(H)" "$(OUT)"
	@echo "captured raw frame: $(OUT)"

jpg: run
	@if command -v ffmpeg >/dev/null 2>&1; then \
		ffmpeg -y -f rawvideo -pixel_format yuyv422 -video_size $(W)x$(H) -i "$(OUT)" -frames:v 1 "$(OUT_JPG)" >/dev/null 2>&1; \
		echo "converted jpg: $(OUT_JPG)"; \
	else \
		echo "ffmpeg not found, skip jpg conversion"; \
	fi

list:
	@echo "sources:"
	@for f in $(SRC); do echo "  - $$f"; done
	@echo "binaries:"
	@for t in $(TARGETS); do echo "  - $$t"; done

clean:
	rm -rf $(BIN_DIR)
