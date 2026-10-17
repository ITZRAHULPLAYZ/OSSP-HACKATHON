# ==============================================================================
# Makefile — Smart CPU Scheduler with Dynamic Priority Adjustment
# ==============================================================================
#
# Targets:
#   make              → build ./scheduler  (or scheduler.exe on Windows)
#   make test         → build & run test suite
#   make clean        → remove build artefacts
#   make run          → build + run with defaults (MLFQ, 8 procs, 120 ticks)
#   make compare      → build + run comparison of all 4 algorithms
#   make demo-rr      → Round Robin demo
#   make demo-srtf    → SRTF demo
#   make demo-pri     → Priority Preemptive demo

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -Iinclude
SRCDIR  := src
TSTDIR  := tests
OBJDIR  := obj

SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Platform detection
ifeq ($(OS),Windows_NT)
    TARGET   := scheduler.exe
    TEST_BIN := test_runner.exe
    MKDIR    := mkdir
    RM       := del /Q /F
    RMDIR    := rmdir /S /Q
    SEP      := $(OBJDIR)\\
else
    TARGET   := scheduler
    TEST_BIN := test_runner
    MKDIR    := mkdir -p
    RM       := rm -f
    RMDIR    := rm -rf
    SEP      := $(OBJDIR)/
endif

# ==============================================================================
.PHONY: all test clean run compare demo-rr demo-srtf demo-pri

all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Build complete: $(TARGET)"

# Compile each source file
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@$(MKDIR) $(OBJDIR) 2>$(NUL) || true
	$(CC) $(CFLAGS) -c $< -o $@

# ==============================================================================
# Test suite
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TSTDIR)/test_runner.c $(filter-out $(OBJDIR)/main.o, $(OBJS))
	@$(MKDIR) $(OBJDIR) 2>$(NUL) || true
	$(CC) $(CFLAGS) $^ -o $@

# ==============================================================================
# Run targets
run: $(TARGET)
	./$(TARGET) --algo mlfq --procs 8 --ticks 120 --speed 110

compare: $(TARGET)
	./$(TARGET) --compare --procs 10 --ticks 200

demo-rr: $(TARGET)
	./$(TARGET) --algo rr --quantum 5 --procs 8 --ticks 120 --speed 110

demo-srtf: $(TARGET)
	./$(TARGET) --algo srtf --procs 8 --ticks 120 --speed 110

demo-pri: $(TARGET)
	./$(TARGET) --algo priority --procs 8 --ticks 120 --speed 110

# ==============================================================================
clean:
ifeq ($(OS),Windows_NT)
	-$(RMDIR) $(OBJDIR)
	-$(RM) $(TARGET) $(TEST_BIN)
else
	$(RMDIR) $(OBJDIR)
	$(RM) $(TARGET) $(TEST_BIN)
endif
