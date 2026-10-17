# Smart CPU Scheduler with Dynamic Priority Adjustment

> A full simulation of a preemptive OS CPU scheduler in **pure C**, featuring four scheduling algorithms, real-time terminal visualisation, and an adaptive priority engine that eliminates starvation.

---

## ✨ Features

| Feature | Description |
|---|---|
| **4 Algorithms** | MLFQ, Round Robin, Priority Preemptive, SRTF |
| **Dynamic Priority** | Aging boosts, CPU-bound penalty, I/O-bound reward, priority decay |
| **Anti-Starvation** | Aging engine promotes any process waiting > threshold ticks |
| **I/O Simulation** | Processes trigger I/O bursts mid-execution and return to queue |
| **Live Dashboard** | ANSI colour-coded terminal UI with Gantt chart, queue inspector, process table |
| **Algorithm Comparison** | Run all 4 algorithms on the same workload and compare metrics side-by-side |
| **Test Suite** | 10 automated tests covering every subsystem |

---

## 🗂️ Project Structure

```
cpu_scheduler/
├── include/
│   ├── config.h       ← All simulation constants
│   ├── process.h      ← Process Control Block (PCB) definition
│   ├── queue.h        ← FIFO, priority, and MLFQ queue structures
│   ├── stats.h        ← Statistics tracking
│   ├── scheduler.h    ← Core scheduler API
│   └── display.h      ← ANSI terminal dashboard
├── src/
│   ├── process.c      ← Process creation, classification
│   ├── queue.c        ← Queue + MLFQ operations
│   ├── stats.c        ← Metrics collection & reporting
│   ├── scheduler.c    ← All algorithms + priority engine
│   ├── display.c      ← Real-time dashboard rendering
│   └── main.c         ← CLI entry point
├── tests/
│   └── test_runner.c  ← Automated test suite (10 tests)
├── docs/
│   └── report.md      ← Full project report
├── presentation/
│   └── index.html     ← Slide deck (open in browser)
├── Makefile
├── build.bat          ← Windows build script
└── README.md
```

---

## ⚙️ Prerequisites

| Platform | Tool |
|---|---|
| Linux / macOS | `gcc`, `make` |
| Windows | [MinGW-w64](https://www.mingw-w64.org/) (provides `gcc` and `make`) |

> **Windows Terminal** recommended for full ANSI colour support.

---

## 🔨 Build & Run

### Linux / macOS

```bash
# Build
make

# Run (live MLFQ dashboard — default)
make run
# or
./scheduler --algo mlfq --procs 8 --ticks 120

# Compare all 4 algorithms
make compare

# Run tests
make test
```

### Windows

```bat
REM Build
build.bat

REM Run
scheduler.exe --algo mlfq --procs 8 --ticks 120

REM Compare algorithms
scheduler.exe --compare --procs 10

REM Run tests
build.bat test
```

---

## 🎮 CLI Options

```
scheduler [OPTIONS]

  --algo  <mlfq|rr|priority|srtf>   Algorithm        (default: mlfq)
  --procs <N>                        Processes        (default: 8)
  --ticks <N>                        Simulation ticks (default: 120)
  --quantum <N>                      RR time quantum  (default: 5)
  --speed <ms>                       ms per tick      (default: 120)
  --seed  <N>                        RNG seed         (default: time())
  --compare                          Compare all 4 algorithms
  --no-anim                          Disable live dashboard
  --help                             Show help
```

### Example commands

```bash
# MLFQ with 10 processes, faster animation
./scheduler --algo mlfq --procs 10 --ticks 150 --speed 80

# Round Robin quantum=6
./scheduler --algo rr --quantum 6 --procs 8

# Reproducible run (fixed seed)
./scheduler --seed 42 --procs 6 --ticks 100

# Algorithm comparison (no animation)
./scheduler --compare --procs 12 --ticks 200

# Silent output (no animation)
./scheduler --algo srtf --no-anim --procs 6
```

---

## 🧠 Scheduling Algorithms

### MLFQ — Multi-Level Feedback Queue *(default)*
- 4 priority levels with time quanta **4 → 8 → 16 → 32** ticks
- New processes enter at Level 0 (highest priority)
- **CPU-bound** processes that exhaust their quantum → demoted one level
- **I/O-bound** processes that yield early → stay at same level or promoted
- Every `AGING_CHECK_INTERVAL` ticks, any process with age > `STARVATION_THRESHOLD` → promoted to Level 0 + priority boost

### Round Robin
- Single FIFO queue, fixed configurable quantum
- On quantum expiry → preempt and enqueue at tail
- Aging still applied to prevent starvation

### Priority Preemptive
- Always runs the highest `dynamic_priority` process
- A newly arrived high-priority process immediately preempts the CPU
- Aging boosts lower-priority processes to prevent indefinite starvation

### SRTF — Shortest Remaining Time First
- Always runs the process with the **least remaining CPU time**
- Preemption occurs when a newly arrived process is shorter than the running one
- Starvation guard: aging ensures long processes eventually run

---

## 🎯 Dynamic Priority Engine

```
┌─────────────────────────────────────────────────────────┐
│  Every tick:                                             │
│    running_process → priority_decay() (slow decay)       │
│                                                          │
│  On quantum exhaustion (CPU-bound behaviour):            │
│    process.dynamic_priority -= CPU_BOUND_PENALTY (1)     │
│    MLFQ: demote to next level                            │
│                                                          │
│  On I/O yield (I/O-bound behaviour):                     │
│    process.dynamic_priority += IO_BOUND_REWARD (1)       │
│    MLFQ: stay at same level                              │
│                                                          │
│  Every AGING_CHECK_INTERVAL ticks:                       │
│    for each READY process with age > THRESHOLD:          │
│      process.dynamic_priority += AGE_BOOST (2)           │
│      MLFQ: promote to Level 0                            │
│      starvation_count++                                  │
└─────────────────────────────────────────────────────────┘
```

---

## 📊 Metrics Tracked

| Metric | Formula |
|---|---|
| Waiting Time | Turnaround − Burst − I/O time |
| Turnaround Time | Completion − Arrival |
| Response Time | First run − Arrival |
| CPU Utilization | Busy ticks / Total ticks × 100% |
| Throughput | Completed / Total ticks |
| Context Switches | Count of running-process changes |
| Starvation Events | Times aging was triggered |

---

## 🖥️ Dashboard Preview

```
┌────────────────────────────────────────────────────────────────────────────┐
│ CPU SCHEDULER  Algo: MLFQ (Dynamic)   Tick: 0042  CPU: 87.3%  CtxSw: 14  │
├────────────────────────────────────────────────────────────────────────────┤
│ GANTT  [tick  0 →  41]                                                     │
│ ████·█████·████████·███··███████████████·█████████████████████████████    │
│ █ P1:nginx-1  █ P2:vim-2  █ P3:gcc-3  ·idle                              │
├────────────────────────────────────────────────────────────────────────────┤
│ QUEUES                                                                     │
│ Level 0 [Q=4 ] : P3 → P7                                                  │
│ Level 1 [Q=8 ] : P1 → P5 → P2                                             │
│ Level 2 [Q=16] : P6                                                        │
│ Level 3 [Q=32] : (empty)                                                   │
│ I/O Wait       : P4[3t] → P8[6t]                                          │
├────────────────────────────────────────────────────────────────────────────┤
│ PID  NAME          PRI DPR  STATE      WAIT REM  AGE TYPE BOOST           │
│   1  nginx-1         8  10  RUNNING       4  12    0 CPU  ★★              │
│   2  vim-2           5   7  READY         8   6    6 I/O                  │
│   3  gcc-3           3   3  I/O WAIT      2  24    0 MIX  ★               │
├────────────────────────────────────────────────────────────────────────────┤
│ AvgWait=6.2t  AvgTAT=14.1t  AvgResp=3.2t  Util=87%  Aging=3  Stars=2    │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 🧪 Test Suite

```bash
make test        # Linux/Mac
build.bat test   # Windows
```

| # | Test | What it verifies |
|---|---|---|
| 1 | Queue Operations | enqueue, dequeue, remove by PID |
| 2 | MLFQ Operations | insert, pick, demote, promote |
| 3 | Priority Queue | sorted insertion (desc priority) |
| 4 | Anti-starvation Aging | age > threshold → priority boost + level 0 |
| 5 | MLFQ Simulation | CPU-bound process gets penalised |
| 6 | RR Simulation | all 6 processes complete |
| 7 | Priority Preemptive | high-priority finishes before low-priority |
| 8 | SRTF | short job finishes before long job |
| 9 | Stats Correctness | avg_wait and avg_tat computed correctly |
| 10 | Comparison Mode | all 4 algorithms run without crash |

---

## 🌍 Real-World Impact

This scheduler design directly maps to concepts in:
- **Linux CFS** (Completely Fair Scheduler) — uses virtual runtime-based priority
- **Windows NT Scheduler** — multi-level priority queues with aging
- **Cloud autoscalers** — dynamic job priority based on wait time
- **Real-time systems** — deadline-aware priority adjustment

---

## 👥 Team Roles

| Role | Responsibility |
|---|---|
| Architecture | PCB design, queue module, MLFQ structure |
| Algorithm Engineer | MLFQ, RR, Priority, SRTF implementations |
| Priority Engine | Aging, penalty/reward logic, decay |
| Display | ANSI dashboard, Gantt chart, comparison table |
| Testing | Test suite, edge cases, benchmarking |
| Documentation | README, report, presentation |

---

## 📄 License

MIT License — free for academic and educational use.
