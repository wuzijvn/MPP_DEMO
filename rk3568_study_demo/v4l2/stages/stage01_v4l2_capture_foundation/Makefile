CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra
LDFLAGS ?=

# Makefile 设计目标：
# 1) 保持最小依赖，便于新手快速上手；
# 2) all/one/run/list/clean 覆盖常见迭代动作；
# 3) 通过变量覆盖支持脚本化与 CI。

SRC := $(wildcard *.cpp)
BIN_DIR := bin
TARGETS := $(patsubst %.cpp,$(BIN_DIR)/%,$(SRC))

APP ?= stage01_v4l2_capture_main
DEV ?= /dev/video10
W ?= 640
H ?= 480
FRAMES ?= 300
STAMP := $(shell date -u +%Y%m%dT%H%M%SZ)
OUT ?= ../../../artifacts/stage01_capture_$(STAMP).yuyv
OUT_PPM ?= $(OUT:.yuyv=.ppm)

.PHONY: all one run list clean

# 默认构建当前目录下所有可执行目标。
all: $(TARGETS)

# one 只编译 APP 指定目标，适合快速迭代单个入口。
one: $(BIN_DIR)/$(APP)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%: %.cpp | $(BIN_DIR)
	# 这里是最简“单文件 -> 单可执行”规则。
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

run: one
	# run 会先确保输出目录存在，再执行采集命令。
	# 你可用 make run DEV=/dev/video0 W=1280 H=720 FRAMES=120 覆盖默认参数。
	mkdir -p "$(dir $(OUT))"
	./$(BIN_DIR)/$(APP) "$(DEV)" "$(W)" "$(H)" "$(OUT)" "$(OUT_PPM)" "$(FRAMES)"
	@echo "captured raw frame: $(OUT)"
	@echo "captured ppm frame: $(OUT_PPM)"

list:
	@echo "sources:"
	@for f in $(SRC); do echo "  - $$f"; done
	@echo "binaries:"
	@for t in $(TARGETS); do echo "  - $$t"; done

clean:
	rm -rf $(BIN_DIR)
