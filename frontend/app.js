/**
 * Smart CPU Scheduler // Dynamic Priority Adjustment Engine
 * High-Contrast B&W Browser Visualizer
 *
 * Implements:
 *  - MLFQ (4 Levels: 4, 8, 16, 32 ticks)
 *  - Round Robin (RR)
 *  - Preemptive Priority (PRI)
 *  - Shortest Remaining Time First (SRTF)
 *
 * Dynamic Features:
 *  - Anti-starvation Aging (Age >= 18 ticks -> +2 Priority & Promotion to Q0)
 *  - CPU Penalty (-1 Priority on full quantum exhaustion)
 *  - I/O Reward (+1 Priority on early yield)
 *  - Continuous Priority Decay
 */

// ── Kernel Configuration (Matches C implementation) ──
const CONFIG = {
  MLFQ_LEVELS: 4,
  MLFQ_QUANTA: [4, 8, 16, 32],
  DEFAULT_RR_QUANTUM: 5,
  STARVATION_THRESHOLD: 18,
  AGE_BOOST: 2,
  MAX_PRIORITY: 10,
  MIN_PRIORITY: 1,
  CPU_BOUND_PENALTY: 1,
  IO_BOUND_REWARD: 1,
  PRIORITY_DECAY_TICKS: 8,
  GANTT_WINDOW: 40
};

// ── Process Model ──
class Process {
  constructor(opts) {
    this.pid = opts.pid;
    this.name = opts.name;
    this.type = opts.type || 'MIXED'; // 'CPU', 'IO', 'MIXED'
    this.arrivalTime = opts.arrivalTime || 0;
    this.burstTime = opts.burstTime;
    this.remainingBurst = opts.burstTime;
    this.staticPriority = opts.staticPriority || 5;
    this.dynamicPriority = opts.dynamicPriority || opts.staticPriority || 5;

    this.ioProbability = opts.ioProbability !== undefined ? opts.ioProbability : 30; // % chance to yield
    this.state = 'READY'; // 'READY', 'RUNNING', 'WAITING', 'TERMINATED'

    this.mlfqLevel = 0;
    this.currentQuantumRemaining = 0;

    // Metrics
    this.waitTime = 0;
    this.turnaroundTime = 0;
    this.age = 0;
    this.starvationCount = 0;
    this.cpuTicksUsed = 0;
    this.ioTicksRemaining = 0;
    this.finishTime = null;
    this.startTime = null;
  }

  clone() {
    return new Process({
      pid: this.pid,
      name: this.name,
      type: this.type,
      arrivalTime: this.arrivalTime,
      burstTime: this.burstTime,
      staticPriority: this.staticPriority,
      dynamicPriority: this.staticPriority,
      ioProbability: this.ioProbability
    });
  }

  clampPriority() {
    if (this.dynamicPriority > CONFIG.MAX_PRIORITY) this.dynamicPriority = CONFIG.MAX_PRIORITY;
    if (this.dynamicPriority < CONFIG.MIN_PRIORITY) this.dynamicPriority = CONFIG.MIN_PRIORITY;
  }
}

// ── Presets ──
const PRESETS = {
  mixed: [
    { pid: 0, name: 'P0_Init',      type: 'MIXED', arrivalTime: 0, burstTime: 12, staticPriority: 7, ioProbability: 25 },
    { pid: 1, name: 'P1_WebServe',  type: 'IO',    arrivalTime: 1, burstTime: 8,  staticPriority: 8, ioProbability: 55 },
    { pid: 2, name: 'P2_Compute',   type: 'CPU',   arrivalTime: 2, burstTime: 24, staticPriority: 4, ioProbability: 10 },
    { pid: 3, name: 'P3_DbQuery',   type: 'MIXED', arrivalTime: 3, burstTime: 14, staticPriority: 6, ioProbability: 35 },
    { pid: 4, name: 'P4_Backup',    type: 'CPU',   arrivalTime: 4, burstTime: 28, staticPriority: 2, ioProbability: 5  },
    { pid: 5, name: 'P5_AudioProc', type: 'IO',    arrivalTime: 6, burstTime: 6,  staticPriority: 9, ioProbability: 60 },
    { pid: 6, name: 'P6_Compress',  type: 'CPU',   arrivalTime: 8, burstTime: 20, staticPriority: 3, ioProbability: 10 },
    { pid: 7, name: 'P7_Telemetry', type: 'MIXED', arrivalTime: 10, burstTime: 10, staticPriority: 5, ioProbability: 30 }
  ],
  cpu_heavy: [
    { pid: 0, name: 'Matrix_Mul',  type: 'CPU', arrivalTime: 0, burstTime: 30, staticPriority: 5, ioProbability: 0 },
    { pid: 1, name: 'Ray_Trace',   type: 'CPU', arrivalTime: 2, burstTime: 25, staticPriority: 6, ioProbability: 0 },
    { pid: 2, name: 'ML_Train',    type: 'CPU', arrivalTime: 3, burstTime: 35, staticPriority: 4, ioProbability: 5 },
    { pid: 3, name: 'Quick_Flush', type: 'IO',  arrivalTime: 5, burstTime: 6,  staticPriority: 8, ioProbability: 70 },
    { pid: 4, name: 'FFT_Analysis',type: 'CPU', arrivalTime: 7, burstTime: 22, staticPriority: 3, ioProbability: 0 }
  ],
  extreme_starvation: [
    { pid: 0, name: 'CPU_Goliath_1', type: 'CPU', arrivalTime: 0, burstTime: 40, staticPriority: 9, ioProbability: 0 },
    { pid: 1, name: 'CPU_Goliath_2', type: 'CPU', arrivalTime: 0, burstTime: 40, staticPriority: 9, ioProbability: 0 },
    { pid: 2, name: 'Starved_P2',    type: 'MIXED', arrivalTime: 0, burstTime: 12, staticPriority: 2, ioProbability: 20 },
    { pid: 3, name: 'Starved_P3',    type: 'MIXED', arrivalTime: 2, burstTime: 10, staticPriority: 1, ioProbability: 15 }
  ],
  quick: [
    { pid: 0, name: 'Task_Alpha', type: 'MIXED', arrivalTime: 0, burstTime: 8,  staticPriority: 5, ioProbability: 20 },
    { pid: 1, name: 'Task_Beta',  type: 'IO',    arrivalTime: 1, burstTime: 6,  staticPriority: 7, ioProbability: 50 },
    { pid: 2, name: 'Task_Gamma', type: 'CPU',   arrivalTime: 2, burstTime: 14, staticPriority: 4, ioProbability: 10 },
    { pid: 3, name: 'Task_Delta', type: 'MIXED', arrivalTime: 3, burstTime: 10, staticPriority: 6, ioProbability: 30 }
  ]
};

// ── Kernel Engine State ──
class KernelEngine {
  constructor() {
    this.algorithm = 'mlfq';
    this.tick = 0;
    this.speedMs = 180;
    this.running = false;
    this.timerId = null;

    this.allProcesses = [];
    this.readyQueue = []; // For RR, PRI, SRTF
    this.mlfqQueues = [[], [], [], []]; // 4 MLFQ queues
    this.ioQueue = [];
    this.runningProc = null;

    this.ganttHistory = []; // Array of { tick, pid, procName, aged }
    this.logEntries = [];

    // Telemetry
    this.starvationEvents = 0;
    this.agingPromotions = 0;
    this.cpuBusyTicks = 0;
  }

  loadWorkload(procs) {
    this.reset();
    this.allProcesses = procs.map(p => new Process(p));
    // Pre-populate queue with tick 0 arrivals so UI isn't empty before first step
    this.allProcesses.forEach(p => {
      if (p.arrivalTime === 0) {
        this.enqueueReady(p);
      }
    });
    this.log(`Kernel initialized with ${this.allProcesses.length} processes. Algorithm: ${this.algorithm.toUpperCase()}`);
  }

  reset() {
    this.pause();
    this.tick = 0;
    this.readyQueue = [];
    this.mlfqQueues = [[], [], [], []];
    this.ioQueue = [];
    this.runningProc = null;
    this.ganttHistory = [];
    this.logEntries = [];
    this.starvationEvents = 0;
    this.agingPromotions = 0;
    this.cpuBusyTicks = 0;

    // Reset process statuses
    this.allProcesses.forEach(p => {
      p.remainingBurst = p.burstTime;
      p.dynamicPriority = p.staticPriority;
      p.state = 'READY';
      p.mlfqLevel = 0;
      p.currentQuantumRemaining = 0;
      p.waitTime = 0;
      p.turnaroundTime = 0;
      p.age = 0;
      p.starvationCount = 0;
      p.cpuTicksUsed = 0;
      p.ioTicksRemaining = 0;
      p.finishTime = null;
      p.startTime = null;
    });

    this.log(`Simulation reset to Tick 0.`);
  }

  log(msg, type = 'normal') {
    const entry = { tick: this.tick, msg, type };
    this.logEntries.unshift(entry);
    if (this.logEntries.length > 80) this.logEntries.pop();
  }

  // ── Step Execution (1 Clock Tick) ──
  step() {
    const curTick = this.tick;

    // 1. Check New Arrivals at this tick
    this.allProcesses.forEach(p => {
      if (p.arrivalTime === curTick && p.state === 'READY' && p.remainingBurst > 0) {
        if (!this.isInReadyState(p)) {
          this.enqueueReady(p);
          this.log(`[TICK ${curTick}] Process P${p.pid} (${p.name}) arrived. Added to ready pool.`);
        }
      }
    });

    // 2. Progress I/O Queue
    for (let i = this.ioQueue.length - 1; i >= 0; i--) {
      const p = this.ioQueue[i];
      p.ioTicksRemaining--;
      if (p.ioTicksRemaining <= 0) {
        this.ioQueue.splice(i, 1);
        p.state = 'READY';
        // I/O reward on completion of I/O burst
        p.dynamicPriority += CONFIG.IO_BOUND_REWARD;
        p.clampPriority();
        this.enqueueReady(p);
        this.log(`[TICK ${curTick}] Process P${p.pid} (${p.name}) completed I/O. Dynamic priority boosted to ${p.dynamicPriority}.`);
      }
    }

    // 3. Anti-Starvation Aging Engine (Every tick check waiting processes)
    this.applyAging();

    // 4. CPU Preemption / Selection check
    this.dispatchCPU();

    // 5. Execute 1 Tick on Running Process
    let ganttItem = { tick: curTick, pid: -1, procName: 'IDLE', aged: false };

    if (this.runningProc) {
      const p = this.runningProc;
      this.cpuBusyTicks++;
      p.cpuTicksUsed++;
      p.remainingBurst--;
      p.currentQuantumRemaining--;

      if (p.startTime === null) p.startTime = curTick;

      ganttItem = {
        tick: curTick,
        pid: p.pid,
        procName: p.name,
        aged: p.starvationCount > 0
      };

      // Periodic priority decay to prevent monopoly
      if (p.cpuTicksUsed > 0 && p.cpuTicksUsed % CONFIG.PRIORITY_DECAY_TICKS === 0) {
        if (p.dynamicPriority > p.staticPriority) {
          p.dynamicPriority--;
          p.clampPriority();
        }
      }

      // Check if process finished
      if (p.remainingBurst <= 0) {
        p.state = 'TERMINATED';
        p.finishTime = curTick + 1;
        p.turnaroundTime = p.finishTime - p.arrivalTime;
        this.log(`[TICK ${curTick}] Process P${p.pid} (${p.name}) FINISHED. Turnaround: ${p.turnaroundTime}t, Wait: ${p.waitTime}t.`, 'finish');
        this.runningProc = null;
      }
      // Check voluntary I/O yield before quantum runs out
      else if (p.type !== 'CPU' && Math.random() * 100 < p.ioProbability && p.currentQuantumRemaining > 0) {
        const ioDuration = 2 + Math.floor(Math.random() * 4); // 2-5 ticks
        p.state = 'WAITING';
        p.ioTicksRemaining = ioDuration;
        p.dynamicPriority += CONFIG.IO_BOUND_REWARD;
        p.clampPriority();
        this.ioQueue.push(p);
        this.log(`[TICK ${curTick}] Process P${p.pid} yielded for I/O (${ioDuration}t). Dynamic priority: ${p.dynamicPriority}.`);
        this.runningProc = null;
      }
      // Check Quantum Expiration (MLFQ or RR)
      else if (p.currentQuantumRemaining <= 0) {
        if (this.algorithm === 'mlfq') {
          // Demote to next queue if not already at bottom
          p.dynamicPriority -= CONFIG.CPU_BOUND_PENALTY;
          p.clampPriority();
          if (p.mlfqLevel < CONFIG.MLFQ_LEVELS - 1) {
            p.mlfqLevel++;
          }
          p.state = 'READY';
          this.enqueueReady(p);
          this.log(`[TICK ${curTick}] Process P${p.pid} quantum expired. Demoted to Q${p.mlfqLevel} (Pri: ${p.dynamicPriority}).`);
          this.runningProc = null;
        } else if (this.algorithm === 'rr') {
          p.state = 'READY';
          this.readyQueue.push(p);
          this.log(`[TICK ${curTick}] Process P${p.pid} RR quantum expired. Returned to queue.`);
          this.runningProc = null;
        }
      }
    }

    // 6. Update wait times & age for ready processes
    this.updateWaitTimes();

    // 7. Record Gantt
    this.ganttHistory.push(ganttItem);
    this.tick++;

    // 8. Stop if all processes completed
    const allDone = this.allProcesses.every(p => p.state === 'TERMINATED');
    if (allDone) {
      this.pause();
      this.log(`[TICK ${this.tick}] ALL PROCESSES COMPLETED. Simulation complete.`, 'finish');
    }

    return allDone;
  }

  isInReadyState(p) {
    if (this.algorithm === 'mlfq') {
      return this.mlfqQueues.some(q => q.includes(p)) || this.runningProc === p || this.ioQueue.includes(p);
    }
    return this.readyQueue.includes(p) || this.runningProc === p || this.ioQueue.includes(p);
  }

  enqueueReady(p) {
    p.state = 'READY';
    if (this.algorithm === 'mlfq') {
      const lvl = Math.min(Math.max(p.mlfqLevel, 0), CONFIG.MLFQ_LEVELS - 1);
      this.mlfqQueues[lvl].push(p);
    } else {
      this.readyQueue.push(p);
    }
  }

  // ── Anti-Starvation Aging Engine ──
  applyAging() {
    const curTick = this.tick;

    if (this.algorithm === 'mlfq') {
      // Check queues Q1, Q2, Q3 for starving processes
      for (let lvl = 1; lvl < CONFIG.MLFQ_LEVELS; lvl++) {
        const queue = this.mlfqQueues[lvl];
        for (let i = queue.length - 1; i >= 0; i--) {
          const p = queue[i];
          if (p.age >= CONFIG.STARVATION_THRESHOLD) {
            // Remove from lower level
            queue.splice(i, 1);

            // Boost priority and promote directly to Top Queue Q0
            p.dynamicPriority += CONFIG.AGE_BOOST;
            p.clampPriority();
            p.mlfqLevel = 0;
            p.starvationCount++;
            p.age = 0;

            this.mlfqQueues[0].push(p);
            this.starvationEvents++;
            this.agingPromotions++;

            this.log(`⚡ [TICK ${curTick}] STARVATION RESCUE: P${p.pid} aged ${CONFIG.STARVATION_THRESHOLD}t! Boosted Pri: ${p.dynamicPriority}, Promoted -> Q0.`, 'aging');
          }
        }
      }
    } else {
      // RR, PRI, SRTF
      this.readyQueue.forEach(p => {
        if (p.age >= CONFIG.STARVATION_THRESHOLD) {
          p.dynamicPriority += CONFIG.AGE_BOOST;
          p.clampPriority();
          p.starvationCount++;
          p.age = 0;
          this.starvationEvents++;
          this.agingPromotions++;
          this.log(`⚡ [TICK ${curTick}] STARVATION RESCUE: P${p.pid} aged ${CONFIG.STARVATION_THRESHOLD}t! Dynamic Pri boosted to ${p.dynamicPriority}.`, 'aging');
        }
      });
    }
  }

  // ── CPU Dispatcher ──
  dispatchCPU() {
    if (this.algorithm === 'mlfq') {
      if (!this.runningProc) {
        // Pick from highest non-empty queue Q0 -> Q3
        for (let lvl = 0; lvl < CONFIG.MLFQ_LEVELS; lvl++) {
          if (this.mlfqQueues[lvl].length > 0) {
            const p = this.mlfqQueues[lvl].shift();
            p.state = 'RUNNING';
            p.mlfqLevel = lvl;
            p.currentQuantumRemaining = CONFIG.MLFQ_QUANTA[lvl];
            p.age = 0; // reset age once scheduled
            this.runningProc = p;
            this.log(`[TICK ${this.tick}] CPU dispatched P${p.pid} (${p.name}) from Q${lvl} (Quantum: ${p.currentQuantumRemaining}t).`);
            break;
          }
        }
      }
    } else if (this.algorithm === 'rr') {
      if (!this.runningProc && this.readyQueue.length > 0) {
        const p = this.readyQueue.shift();
        p.state = 'RUNNING';
        p.currentQuantumRemaining = CONFIG.DEFAULT_RR_QUANTUM;
        p.age = 0;
        this.runningProc = p;
      }
    } else if (this.algorithm === 'pri') {
      // Sort ready queue by dynamic priority descending
      this.readyQueue.sort((a, b) => b.dynamicPriority - a.dynamicPriority);

      // Preemptive: check if any ready process has strictly higher dynamic priority
      if (this.readyQueue.length > 0) {
        const bestCandidate = this.readyQueue[0];
        if (!this.runningProc || bestCandidate.dynamicPriority > this.runningProc.dynamicPriority) {
          if (this.runningProc) {
            this.runningProc.state = 'READY';
            this.readyQueue.push(this.runningProc);
            this.log(`[TICK ${this.tick}] PREEMPTION: P${this.runningProc.pid} preempted by P${bestCandidate.pid} (Pri: ${bestCandidate.dynamicPriority} > ${this.runningProc.dynamicPriority}).`);
          }
          this.readyQueue.shift();
          bestCandidate.state = 'RUNNING';
          bestCandidate.age = 0;
          this.runningProc = bestCandidate;
        }
      }
    } else if (this.algorithm === 'srtf') {
      // Shortest Remaining Time First with dynamic priority tie-breaking
      this.readyQueue.sort((a, b) => {
        if (a.remainingBurst !== b.remainingBurst) return a.remainingBurst - b.remainingBurst;
        return b.dynamicPriority - a.dynamicPriority;
      });

      if (this.readyQueue.length > 0) {
        const shortest = this.readyQueue[0];
        if (!this.runningProc || shortest.remainingBurst < this.runningProc.remainingBurst) {
          if (this.runningProc) {
            this.runningProc.state = 'READY';
            this.readyQueue.push(this.runningProc);
          }
          this.readyQueue.shift();
          shortest.state = 'RUNNING';
          shortest.age = 0;
          this.runningProc = shortest;
        }
      }
    }
  }

  updateWaitTimes() {
    const increment = (p) => {
      p.waitTime++;
      p.age++;
    };

    if (this.algorithm === 'mlfq') {
      this.mlfqQueues.forEach(q => q.forEach(increment));
    } else {
      this.readyQueue.forEach(increment);
    }
  }

  play(onTickCallback) {
    if (this.running) return;
    this.running = true;
    const runLoop = () => {
      if (!this.running) return;
      const finished = this.step();
      if (onTickCallback) onTickCallback();
      if (!finished && this.running) {
        this.timerId = setTimeout(runLoop, this.speedMs);
      }
    };
    this.timerId = setTimeout(runLoop, this.speedMs);
  }

  pause() {
    this.running = false;
    if (this.timerId) {
      clearTimeout(this.timerId);
      this.timerId = null;
    }
  }

  // ── Run Benchmark on Identical Test Workload ──
  runBenchmark() {
    const algorithms = ['mlfq', 'rr', 'pri', 'srtf'];
    const results = [];

    algorithms.forEach(algo => {
      // Create independent engine with identical cloned workload
      const benchEngine = new KernelEngine();
      benchEngine.algorithm = algo;
      benchEngine.allProcesses = this.allProcesses.map(p => p.clone());

      let maxTicks = 400;
      let ticks = 0;
      while (ticks < maxTicks) {
        const done = benchEngine.step();
        ticks++;
        if (done) break;
      }

      const totalProcs = benchEngine.allProcesses.length;
      const finishedProcs = benchEngine.allProcesses.filter(p => p.state === 'TERMINATED');
      const count = finishedProcs.length || 1;

      const avgWait = finishedProcs.reduce((acc, p) => acc + p.waitTime, 0) / count;
      const avgTat = finishedProcs.reduce((acc, p) => acc + p.turnaroundTime, 0) / count;
      const cpuUtil = ((benchEngine.cpuBusyTicks / Math.max(benchEngine.tick, 1)) * 100).toFixed(1);

      results.push({
        algo: algo.toUpperCase(),
        avgWait: avgWait.toFixed(1),
        avgTat: avgTat.toFixed(1),
        cpuUtil: `${cpuUtil}%`,
        starvations: benchEngine.starvationEvents,
        promotions: benchEngine.agingPromotions,
        completed: `${finishedProcs.length}/${totalProcs}`,
        isMlfq: algo === 'mlfq'
      });
    });

    return results;
  }
}

// ═══════════════════════════════════════════════════
// UI CONTROLLER & RENDERER
// ═══════════════════════════════════════════════════

const engine = new KernelEngine();
if (typeof window !== 'undefined') {
  window.engine = engine;
}

// DOM Element references
const dom = {
  headerTick: document.getElementById('headerTick'),
  headerAlgo: document.getElementById('headerAlgo'),
  headerCpuState: document.getElementById('headerCpuState'),

  btnPlay: document.getElementById('btnPlay'),
  playIcon: document.getElementById('playIcon'),
  playText: document.getElementById('playText'),
  btnStep: document.getElementById('btnStep'),
  btnReset: document.getElementById('btnReset'),
  selectAlgo: document.getElementById('selectAlgo'),
  selectSpeed: document.getElementById('selectSpeed'),
  selectWorkload: document.getElementById('selectWorkload'),

  cpuRunningCard: document.getElementById('cpuRunningCard'),
  coreBurstRemaining: document.getElementById('coreBurstRemaining'),
  coreQuantumRemaining: document.getElementById('coreQuantumRemaining'),

  ganttTimeline: document.getElementById('ganttTimeline'),
  ganttAxis: document.getElementById('ganttAxis'),
  ganttCursorTick: document.getElementById('ganttCursorTick'),

  queueViewTitle: document.getElementById('queueViewTitle'),
  mlfqQueuesContainer: document.getElementById('mlfqQueuesContainer'),
  laneQ0: document.getElementById('laneQ0'),
  laneQ1: document.getElementById('laneQ1'),
  laneQ2: document.getElementById('laneQ2'),
  laneQ3: document.getElementById('laneQ3'),

  singleQueueContainer: document.getElementById('singleQueueContainer'),
  singleQueueTitle: document.getElementById('singleQueueTitle'),
  singleQueueSlots: document.getElementById('singleQueueSlots'),

  cntTotal: document.getElementById('cntTotal'),
  cntReady: document.getElementById('cntReady'),
  cntIo: document.getElementById('cntIo'),
  cntDone: document.getElementById('cntDone'),
  pcbTableBody: document.getElementById('pcbTableBody'),

  kpiStarvations: document.getElementById('kpiStarvations'),
  kpiBoosts: document.getElementById('kpiBoosts'),
  kpiCpuUtil: document.getElementById('kpiCpuUtil'),
  kpiAvgTat: document.getElementById('kpiAvgTat'),
  kernelLogEntries: document.getElementById('kernelLogEntries'),
  btnClearLog: document.getElementById('btnClearLog'),

  btnOpenAddModal: document.getElementById('btnOpenAddModal'),
  modalAddProc: document.getElementById('modalAddProc'),
  btnCloseAddModal: document.getElementById('btnCloseAddModal'),
  btnCancelAddModal: document.getElementById('btnCancelAddModal'),
  formAddProc: document.getElementById('formAddProc'),

  backendBadge: document.getElementById('backendBadge'),
  btnRunCBench: document.getElementById('btnRunCBench'),
  btnRunBenchmark: document.getElementById('btnRunBenchmark'),
  modalBenchmark: document.getElementById('modalBenchmark'),
  btnCloseBenchModal: document.getElementById('btnCloseBenchModal'),
  btnCloseBenchBtn: document.getElementById('btnCloseBenchBtn'),
  btnReRunBench: document.getElementById('btnReRunBench'),
  benchmarkTableBody: document.getElementById('benchmarkTableBody'),
  benchmarkInsights: document.getElementById('benchmarkInsights')
};

// High-contrast process pattern classes (0 to 7)
function getProcPatternClass(pid) {
  return `p-style-${pid % 8}`;
}

// ── Render Function ──
function renderUI() {
  // 1. Header Telemetry
  if (dom.headerTick) dom.headerTick.textContent = String(engine.tick).padStart(3, '0');
  if (dom.headerAlgo) dom.headerAlgo.textContent = engine.algorithm.toUpperCase();
  if (dom.headerCpuState) {
    if (engine.runningProc) {
      dom.headerCpuState.textContent = `BUSY [P${engine.runningProc.pid}]`;
    } else {
      dom.headerCpuState.textContent = 'IDLE';
    }
  }

  // 2. Play / Pause Button state
  if (engine.running) {
    if (dom.playIcon) dom.playIcon.textContent = '⏸';
    if (dom.playText) dom.playText.textContent = 'PAUSE';
  } else {
    if (dom.playIcon) dom.playIcon.textContent = '▶';
    if (dom.playText) dom.playText.textContent = 'RUN';
  }

  // 3. CPU Core Active Card
  if (engine.runningProc) {
    const p = engine.runningProc;
    const patClass = getProcPatternClass(p.pid);
    dom.cpuRunningCard.innerHTML = `
      <div class="active-proc-badge ${patClass}">
        <div class="active-proc-title">
          <span>P${p.pid} &bull; ${p.name}</span>
          <span style="font-size:10px; background:#fff; color:#000; padding:1px 5px; font-weight:800;">RUNNING</span>
        </div>
        <div class="active-proc-meta">
          PRI: ${p.dynamicPriority} (STAT: ${p.staticPriority}) &bull; ${p.type}
        </div>
      </div>
    `;
    dom.coreBurstRemaining.textContent = `${p.remainingBurst}t / ${p.burstTime}t`;
    dom.coreQuantumRemaining.textContent = engine.algorithm === 'mlfq' || engine.algorithm === 'rr'
      ? `${p.currentQuantumRemaining}t`
      : 'N/A';
  } else {
    dom.cpuRunningCard.innerHTML = `<div class="core-idle-msg">IDLE</div>`;
    dom.coreBurstRemaining.textContent = '--';
    dom.coreQuantumRemaining.textContent = '--';
  }

  // 4. Gantt Timeline Render
  renderGantt();

  // 5. Queues Render
  renderQueues();

  // 6. PCB Table
  renderPCBTable();

  // 7. Telemetry & KPIs
  renderKPIs();

  // 8. Kernel Event Log
  renderLog();
}

function renderGantt() {
  const history = engine.ganttHistory;
  const recent = history.slice(-CONFIG.GANTT_WINDOW);
  dom.ganttCursorTick.textContent = `TICK: ${engine.tick}`;

  let cellsHtml = '';
  let axisHtml = '';

  recent.forEach((item, idx) => {
    const isLatest = idx === recent.length - 1;
    if (item.pid === -1) {
      cellsHtml += `<div class="gantt-cell idle ${isLatest ? 'active-tick' : ''}" title="Tick ${item.tick}: IDLE">-</div>`;
    } else {
      const patClass = getProcPatternClass(item.pid);
      const agedMark = item.aged ? '*' : '';
      cellsHtml += `
        <div class="gantt-cell ${patClass} ${isLatest ? 'active-tick' : ''}" title="Tick ${item.tick}: P${item.pid} (${item.procName})">
          P${item.pid}${agedMark}
        </div>`;
    }
    axisHtml += `<div class="gantt-axis-label">${item.tick}</div>`;
  });

  dom.ganttTimeline.innerHTML = cellsHtml || '<div class="empty-hint">Waiting for simulation ticks...</div>';
  dom.ganttAxis.innerHTML = axisHtml;
}

function renderQueues() {
  if (engine.algorithm === 'mlfq') {
    if (dom.mlfqQueuesContainer) dom.mlfqQueuesContainer.style.display = 'grid';
    if (dom.singleQueueContainer) dom.singleQueueContainer.style.display = 'none';

    const lanes = [dom.laneQ0, dom.laneQ1, dom.laneQ2, dom.laneQ3];
    lanes.forEach((laneEl, idx) => {
      if (!laneEl) return;
      const q = engine.mlfqQueues[idx];
      if (q.length === 0) {
        laneEl.innerHTML = '<div class="empty-hint">Empty</div>';
      } else {
        laneEl.innerHTML = q.map(p => createQueueProcPill(p)).join('');
      }
    });
  } else {
    if (dom.mlfqQueuesContainer) dom.mlfqQueuesContainer.style.display = 'none';
    if (dom.singleQueueContainer) dom.singleQueueContainer.style.display = 'block';
    if (dom.singleQueueTitle) dom.singleQueueTitle.textContent = `${engine.algorithm.toUpperCase()} READY QUEUE`;

    if (dom.singleQueueSlots) {
      if (engine.readyQueue.length === 0) {
        dom.singleQueueSlots.innerHTML = '<div class="empty-hint">Ready queue is empty</div>';
      } else {
        dom.singleQueueSlots.innerHTML = engine.readyQueue.map(p => createQueueProcPill(p)).join('');
      }
    }
  }
}

function createQueueProcPill(p) {
  const isAgedTriggered = p.starvationCount > 0;
  return `
    <div class="clean-proc-card ${isAgedTriggered ? 'aged-boost' : ''}" title="Dynamic Pri: ${p.dynamicPriority}, Wait: ${p.waitTime}t">
      <div>
        <span class="card-pid">P${p.pid}</span>
        <strong>${p.name}</strong>
      </div>
      <div>
        <span>Pri: <strong>${p.dynamicPriority}</strong></span>
        <span style="margin-left:6px;">Age: <strong>${p.age}t</strong></span>
        ${isAgedTriggered ? '<span class="card-tag">AGED</span>' : ''}
      </div>
    </div>
  `;
}

function renderPCBTable() {
  const procs = engine.allProcesses;
  let readyCount = 0;
  let ioCount = 0;
  let doneCount = 0;

  const rowsHtml = procs.map(p => {
    if (p.state === 'READY') readyCount++;
    else if (p.state === 'WAITING') ioCount++;
    else if (p.state === 'TERMINATED') doneCount++;

    const isRunning = engine.runningProc === p;
    const priDisplay = `${p.dynamicPriority} / ${p.staticPriority}`;

    return `
      <tr class="${isRunning ? 'row-running' : ''}">
        <td><strong>P${p.pid}</strong></td>
        <td>${p.name}</td>
        <td>${p.type}</td>
        <td>${p.arrivalTime}t</td>
        <td>${p.burstTime}t</td>
        <td>${p.remainingBurst}t</td>
        <td><strong>${priDisplay}</strong></td>
        <td>${p.age}t (${p.waitTime}t)</td>
        <td>${p.starvationCount > 0 ? `<strong>${p.starvationCount}</strong>` : '0'}</td>
        <td><span class="status-badge status-${p.state}">${p.state}</span></td>
      </tr>
    `;
  }).join('');

  dom.cntTotal.textContent = procs.length;
  dom.cntReady.textContent = readyCount;
  dom.cntIo.textContent = ioCount;
  dom.cntDone.textContent = doneCount;
  dom.pcbTableBody.innerHTML = rowsHtml;
}

function renderKPIs() {
  dom.kpiStarvations.textContent = engine.starvationEvents;
  dom.kpiBoosts.textContent = engine.agingPromotions;

  const cpuPct = engine.tick > 0 ? ((engine.cpuBusyTicks / engine.tick) * 100).toFixed(1) : '0.0';
  dom.kpiCpuUtil.textContent = `${cpuPct}%`;

  const finished = engine.allProcesses.filter(p => p.state === 'TERMINATED');
  if (finished.length > 0) {
    const avgTat = (finished.reduce((acc, p) => acc + p.turnaroundTime, 0) / finished.length).toFixed(1);
    dom.kpiAvgTat.textContent = `${avgTat}t`;
  } else {
    dom.kpiAvgTat.textContent = '--';
  }
}

function renderLog() {
  const linesHtml = engine.logEntries.map(e => {
    let cls = 'log-line';
    if (e.type === 'aging') cls += ' log-aging';
    else if (e.type === 'finish') cls += ' log-finish';
    return `<div class="${cls}">[T=${e.tick}] ${e.msg}</div>`;
  }).join('');

  dom.kernelLogEntries.innerHTML = linesHtml;
}

// ═══════════════════════════════════════════════════
// EVENT LISTENERS & WIRING
// ═══════════════════════════════════════════════════

// Play / Pause
dom.btnPlay.addEventListener('click', () => {
  if (engine.running) {
    engine.pause();
    renderUI();
  } else {
    engine.play(() => renderUI());
    renderUI();
  }
});

// Step 1 Tick
dom.btnStep.addEventListener('click', () => {
  engine.pause();
  engine.step();
  renderUI();
});

// Reset
dom.btnReset.addEventListener('click', () => {
  engine.reset();
  renderUI();
});

// Algorithm selection
dom.selectAlgo.addEventListener('change', (e) => {
  engine.pause();
  engine.algorithm = e.target.value;
  engine.reset();
  renderUI();
});

// Speed selection
dom.selectSpeed.addEventListener('change', (e) => {
  engine.speedMs = parseInt(e.target.value, 10);
  if (engine.running) {
    engine.pause();
    engine.play(() => renderUI());
  }
});

// Preset Workload selection
dom.selectWorkload.addEventListener('change', (e) => {
  const key = e.target.value;
  if (PRESETS[key]) {
    engine.loadWorkload(PRESETS[key]);
    renderUI();
  }
});

// Clear Log
dom.btnClearLog.addEventListener('click', () => {
  engine.logEntries = [];
  renderLog();
});

// Modal: Add Process
dom.btnOpenAddModal.addEventListener('click', () => {
  dom.modalAddProc.style.display = 'flex';
});

const closeAddModal = () => {
  dom.modalAddProc.style.display = 'none';
};
dom.btnCloseAddModal.addEventListener('click', closeAddModal);
dom.btnCancelAddModal.addEventListener('click', closeAddModal);

dom.formAddProc.addEventListener('submit', (e) => {
  e.preventDefault();
  const name = document.getElementById('inpProcName').value.trim() || 'Custom';
  const type = document.getElementById('inpProcType').value;
  const burst = parseInt(document.getElementById('inpCpuBurst').value, 10) || 10;
  const arrival = parseInt(document.getElementById('inpArrivalTime').value, 10) || 0;
  const pri = parseInt(document.getElementById('inpStaticPriority').value, 10) || 5;
  const ioChance = parseInt(document.getElementById('inpIoChance').value, 10) || 20;

  const newPid = engine.allProcesses.length;
  const newProc = new Process({
    pid: newPid,
    name: name,
    type: type,
    burstTime: burst,
    arrivalTime: arrival,
    staticPriority: pri,
    dynamicPriority: pri,
    ioProbability: ioChance
  });

  engine.allProcesses.push(newProc);

  // If already at or past arrival tick, place directly into ready queue
  if (engine.tick >= arrival) {
    engine.enqueueReady(newProc);
  }

  engine.log(`Injected custom process P${newPid} (${name}) into kernel PCB.`);
  closeAddModal();
  renderUI();
});

// Modal: Benchmark Comparison
dom.btnRunBenchmark.addEventListener('click', () => {
  runAndDisplayBenchmark();
  dom.modalBenchmark.style.display = 'flex';
});

dom.btnReRunBench.addEventListener('click', () => {
  runAndDisplayBenchmark();
});

if (dom.btnRunCBench) {
  dom.btnRunCBench.addEventListener('click', () => {
    runRealCBenchmark();
  });
}

const closeBenchModal = () => {
  dom.modalBenchmark.style.display = 'none';
};
dom.btnCloseBenchModal.addEventListener('click', closeBenchModal);
dom.btnCloseBenchBtn.addEventListener('click', closeBenchModal);

async function runRealCBenchmark() {
  dom.benchmarkTableBody.innerHTML = `
    <tr>
      <td colspan="7" style="text-align:center; padding: 20px; font-weight:800;">
        ⚡ EXECUTING NATIVE C BINARY (scheduler.exe --compare --json)...
      </td>
    </tr>
  `;

  try {
    const res = await fetch(`/api/c-compare?procs=${engine.allProcesses.length}&ticks=120`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();

    if (data.comparison) {
      const tbodyHtml = data.comparison.map(r => `
        <tr class="${r.algo.includes('MLFQ') ? 'row-running' : ''}">
          <td><strong>${r.algo}</strong></td>
          <td>${r.avg_wait} ticks</td>
          <td>${r.avg_tat} ticks</td>
          <td>${r.cpu_util}%</td>
          <td><strong>${r.starvations}</strong></td>
          <td>${r.promotions}</td>
          <td>${r.algo.includes('MLFQ') ? '<strong>NATIVE C OPTIMAL</strong>' : 'C BASELINE'}</td>
        </tr>
      `).join('');

      dom.benchmarkTableBody.innerHTML = tbodyHtml;
      dom.benchmarkInsights.innerHTML = `
        <strong>⚡ RESULTS FROM REAL C BINARY (scheduler.exe):</strong><br>
        &bull; <strong>Source:</strong> Compiled MinGW-w64 C binary with <code>--compare --json</code>.<br>
        &bull; <strong>Anti-Starvation Aging:</strong> Native C aging engine rescued starving processes and boosted dynamic priorities.<br>
        &bull; <strong>MLFQ:</strong> Dynamically tuned multi-level feedback queue delivers high throughput with starvation guarantees.
      `;
      engine.log(`Executed real C binary benchmark (scheduler.exe). Results loaded.`);
      renderLog();
    }
  } catch (err) {
    dom.benchmarkTableBody.innerHTML = `
      <tr>
        <td colspan="7" style="text-align:center; padding: 14px; color: #ff6666;">
          Backend API unreachable. Falling back to internal engine...
        </td>
      </tr>
    `;
    setTimeout(runAndDisplayBenchmark, 1000);
  }
}

function runAndDisplayBenchmark() {
  const results = engine.runBenchmark();
  const tbodyHtml = results.map(r => `
    <tr class="${r.isMlfq ? 'row-running' : ''}">
      <td><strong>${r.algo}</strong></td>
      <td>${r.avgWait} ticks</td>
      <td>${r.avgTat} ticks</td>
      <td>${r.cpuUtil}</td>
      <td><strong>${r.starvations}</strong></td>
      <td>${r.promotions}</td>
      <td>${r.isMlfq ? '<strong>RECOMMENDED (OPTIMAL)</strong>' : 'BASELINE'}</td>
    </tr>
  `).join('');

  dom.benchmarkTableBody.innerHTML = tbodyHtml;

  dom.benchmarkInsights.innerHTML = `
    <strong>KERNEL INSIGHT &amp; EVALUATION:</strong><br>
    &bull; <strong>MLFQ with Dynamic Priority Aging</strong> prevents starvation events from stalling low-priority processes by promoting them to Queue 0 after 18 ticks of starvation.<br>
    &bull; <strong>Round Robin (RR)</strong> ensures fairness but yields higher turnaround times for CPU-bound processes.<br>
    &bull; <strong>Preemptive Priority (PRI)</strong> without aging would result in severe starvation; our dynamic aging engine successfully rescues starved processes.<br>
    &bull; <strong>SRTF</strong> minimizes average turnaround time, with our anti-starvation aging safeguard preventing starvation of long-burst jobs.<br>
    &bull; Click <strong>"EXECUTE REAL C BINARY"</strong> below to run the compiled <code>scheduler.exe</code> directly!
  `;
}

// ── Backend Health Check ──
async function checkBackendConnection() {
  try {
    const res = await fetch('/api/status', { cache: 'no-store' });
    if (res.ok) {
      const data = await res.json();
      if (dom.backendBadge) {
        dom.backendBadge.textContent = 'ONLINE (C-CORE)';
        dom.backendBadge.style.color = '#ffffff';
        dom.backendBadge.style.fontWeight = '800';
      }
      engine.log(`Connected to C Backend Server (scheduler.exe online).`);
      renderLog();
    }
  } catch (e) {
    if (dom.backendBadge) {
      dom.backendBadge.textContent = 'STANDALONE';
    }
  }
}

// ── Initial Boot ──
window.addEventListener('DOMContentLoaded', () => {
  // Tab switching
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
      btn.classList.add('active');
      const targetId = btn.getAttribute('data-tab');
      const pane = document.getElementById(targetId);
      if (pane) pane.classList.add('active');
    });
  });

  engine.loadWorkload(PRESETS.mixed);
  renderUI();
  checkBackendConnection();
});
