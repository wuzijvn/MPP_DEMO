CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra
LDFLAGS ?= -lpthread

# Makefile 说明：
# 1) 与 Stage01 保持同样命令习惯，减少切换成本；
# 2) 默认链接 pthread，因为 Stage02 有 writer 线程；
# 3) run 目标默认按 duration 运行，贴合稳定性测试场景。

SRC := $(wildcard *.cpp)
BIN_DIR := bin
TARGETS := $(patsubst %.cpp,$(BIN_DIR)/%,$(SRC))

APP ?= stage02_v4l2_controls_stability_main
DEV ?= /dev/video0
W ?= 640
H ?= 480
DURATION ?= 60
STAMP := $(shell date -u +%Y%m%dT%H%M%SZ)
OUT_DIR ?= ../../../artifacts/stage02_run_$(STAMP)

.PHONY: all one run list clean

# 构建全部目标（当前 stage 下所有 .cpp）。
all: $(TARGETS)

# one 仅构建 APP 指定目标。
one: $(BIN_DIR)/$(APP)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%: %.cpp | $(BIN_DIR)
	# 单文件编译规则，保持可读、可改、可移植。
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

run: one
	# Stage02 典型运行方式：按时长跑稳定性，并周期性 dump 原始帧。
	mkdir -p "$(OUT_DIR)"
	./$(BIN_DIR)/$(APP) "$(DEV)" "$(W)" "$(H)" --duration-sec="$(DURATION)" --dump-every=120 --out-dir="$(OUT_DIR)"
	@echo "stage02 outputs: $(OUT_DIR)"

list:
	@echo "sources:"
	@for f in $(SRC); do echo "  - $$f"; done
	@echo "binaries:"
	@for t in $(TARGETS); do echo "  - $$t"; done

clean:
	rm -rf $(BIN_DIR)
