// Global State
let simSteps = [];
let simStats = {};
let currentStepIndex = -1;
let autoplayIntervalId = null;

// DOM Elements
const configForm = document.getElementById("config-form");
const pagesInput = document.getElementById("pages-input");
const framesInput = document.getElementById("frames-input");
const pageSizeSelect = document.getElementById("pagesize-input");
const tlbSizeInput = document.getElementById("tlbsize-input");
const algorithmSelect = document.getElementById("algorithm-input");
const traceSourceSelect = document.getElementById("trace-source");
const predefinedTraceGroup = document.getElementById("predefined-trace-group");
const customTraceGroup = document.getElementById("custom-trace-group");
const traceFileSelect = document.getElementById("trace-file-select");
const customTraceInput = document.getElementById("custom-trace-input");
const btnLoadSim = document.getElementById("btn-load-sim");

const btnPrev = document.getElementById("btn-prev");
const btnNext = document.getElementById("btn-next");
const btnAutoplay = document.getElementById("btn-autoplay");
const btnRunAll = document.getElementById("btn-runall");
const speedSlider = document.getElementById("speed-slider");
const speedValDisplay = document.getElementById("speed-val");
const currentStepDisplay = document.getElementById("current-step-display");
const totalStepsDisplay = document.getElementById("total-steps-display");
const progressBar = document.getElementById("progress-bar");

const tlbGrid = document.getElementById("tlb-grid");
const frameGrid = document.getElementById("frame-grid");
const pageTableBody = document.getElementById("pagetable-body");
const clockHandValBadge = document.getElementById("clock-hand-val");

const statAccesses = document.getElementById("stat-accesses");
const statRW = document.getElementById("stat-rw");
const statTlbHits = document.getElementById("stat-tlb-hits");
const statTlbMisses = document.getElementById("stat-tlb-misses");
const statPageFaults = document.getElementById("stat-page-faults");
const statEvictions = document.getElementById("stat-evictions");
const statDirty = document.getElementById("stat-dirty");
const tlbGaugeFill = document.getElementById("tlb-gauge-fill");
const tlbGaugeTxt = document.getElementById("tlb-gauge-txt");
const pfGaugeFill = document.getElementById("pf-gauge-fill");
const pfGaugeTxt = document.getElementById("pf-gauge-txt");

const mathBreakdownPanel = document.getElementById("math-breakdown-panel");
const consoleLogsContainer = document.getElementById("console-logs-container");

const errorOverlay = document.getElementById("error-overlay");
const btnCloseError = document.getElementById("btn-close-error");
const errorMessageText = document.getElementById("error-message-text");

// Page Initialisation
document.addEventListener("DOMContentLoaded", () => {
  loadTraceFiles();
  setupEventListeners();
});

// Event Listeners Configuration
function setupEventListeners() {
  // Toggle Trace Source (Predefined vs Custom Textarea)
  traceSourceSelect.addEventListener("change", () => {
    if (traceSourceSelect.value === "predefined") {
      predefinedTraceGroup.classList.remove("hidden");
      customTraceGroup.classList.add("hidden");
    } else {
      predefinedTraceGroup.classList.add("hidden");
      customTraceGroup.classList.remove("hidden");
    }
  });

  // Config Form Submission
  configForm.addEventListener("submit", (e) => {
    e.preventDefault();
    triggerSimulation();
  });

  // Playback Navigation
  btnPrev.addEventListener("click", stepBackward);
  btnNext.addEventListener("click", stepForward);
  btnRunAll.addEventListener("click", jumpToEnd);
  btnAutoplay.addEventListener("click", toggleAutoplay);

  // Speed Slider
  speedSlider.addEventListener("input", () => {
    speedValDisplay.textContent = speedSlider.value;
    if (autoplayIntervalId) {
      // Restart interval with new speed
      stopAutoplay();
      startAutoplay();
    }
  });

  // Error modal closing
  btnCloseError.addEventListener("click", () => {
    errorOverlay.classList.add("hidden");
  });
  errorOverlay.addEventListener("click", (e) => {
    if (e.target === errorOverlay) {
      errorOverlay.classList.add("hidden");
    }
  });
}

// Load pre-defined trace files list
async function loadTraceFiles() {
  try {
    const res = await fetch("/api/traces");
    if (!res.ok) throw new Error("Failed to fetch trace files listing");
    const files = await res.json();
    
    traceFileSelect.innerHTML = "";
    files.forEach(file => {
      const option = document.createElement("option");
      option.value = file;
      option.textContent = file;
      traceFileSelect.appendChild(option);
    });
  } catch (err) {
    console.error("Error loading traces:", err);
    document.getElementById("backend-status").textContent = "Connection Error";
    document.querySelector(".status-dot").className = "status-dot red";
  }
}

// Call backend API /api/simulate
async function triggerSimulation() {
  // Stop autoplay if running
  stopAutoplay();

  const reqBody = {
    pages: parseInt(pagesInput.value, 10),
    frames: parseInt(framesInput.value, 10),
    page_size: parseInt(pageSizeSelect.value, 10),
    tlb_size: parseInt(tlbSizeInput.value, 10),
    algorithm: algorithmSelect.value
  };

  if (traceSourceSelect.value === "predefined") {
    reqBody.trace_file = traceFileSelect.value;
  } else {
    reqBody.custom_trace = customTraceInput.value;
  }

  btnLoadSim.disabled = true;
  btnLoadSim.innerHTML = '<i class="fa-solid fa-spinner fa-spin"></i> Running...';

  try {
    const res = await fetch("/api/simulate", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify(reqBody)
    });

    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.detail || "Simulation failed due to unknown error.");
    }

    simSteps = data.steps;
    simStats = data.statistics || data.stats;
    
    if (simSteps.length === 0) {
      throw new Error("Simulation trace generated 0 accesses.");
    }

    // Reset current step pointer
    currentStepIndex = 0;
    
    // Enable playback buttons
    btnPrev.disabled = true;
    btnNext.disabled = simSteps.length <= 1;
    btnAutoplay.disabled = simSteps.length <= 1;
    btnRunAll.disabled = simSteps.length <= 1;

    // Refresh UI display
    updatePlaybackDisplay();
    renderStepState();
    
  } catch (err) {
    console.error("Simulation error:", err);
    showError(err.message);
  } finally {
    btnLoadSim.disabled = false;
    btnLoadSim.innerHTML = '<i class="fa-solid fa-arrow-rotate-right"></i> Initialize & Run';
  }
}

// Show error overlay dialog
function showError(message) {
  errorMessageText.textContent = message;
  errorOverlay.classList.remove("hidden");
}

// Update step indexes and progress bars
function updatePlaybackDisplay() {
  currentStepDisplay.textContent = currentStepIndex + 1;
  totalStepsDisplay.textContent = simSteps.length;
  
  const pct = simSteps.length > 1 ? (currentStepIndex / (simSteps.length - 1)) * 100 : 100;
  progressBar.style.width = `${pct}%`;
}

// Playback actions
function stepForward() {
  if (currentStepIndex < simSteps.length - 1) {
    currentStepIndex++;
    updatePlaybackDisplay();
    renderStepState();
    
    btnPrev.disabled = false;
    if (currentStepIndex === simSteps.length - 1) {
      btnNext.disabled = true;
      stopAutoplay();
    }
  }
}

function stepBackward() {
  if (currentStepIndex > 0) {
    currentStepIndex--;
    updatePlaybackDisplay();
    renderStepState();
    
    btnNext.disabled = false;
    btnAutoplay.disabled = false;
    if (currentStepIndex === 0) {
      btnPrev.disabled = true;
    }
  }
}

function jumpToEnd() {
  stopAutoplay();
  currentStepIndex = simSteps.length - 1;
  updatePlaybackDisplay();
  renderStepState();
  
  btnPrev.disabled = false;
  btnNext.disabled = true;
}

function toggleAutoplay() {
  if (autoplayIntervalId) {
    stopAutoplay();
  } else {
    startAutoplay();
  }
}

function startAutoplay() {
  btnAutoplay.innerHTML = '<i class="fa-solid fa-pause"></i>';
  btnAutoplay.title = "Pause";
  
  const delay = parseInt(speedSlider.value, 10);
  
  // If we are at the end, reset to step 0
  if (currentStepIndex === simSteps.length - 1) {
    currentStepIndex = 0;
    updatePlaybackDisplay();
    renderStepState();
    btnPrev.disabled = true;
    btnNext.disabled = false;
  }
  
  autoplayIntervalId = setInterval(() => {
    stepForward();
  }, delay);
}

function stopAutoplay() {
  if (autoplayIntervalId) {
    clearInterval(autoplayIntervalId);
    autoplayIntervalId = null;
  }
  btnAutoplay.innerHTML = '<i class="fa-solid fa-play"></i>';
  btnAutoplay.title = "Autoplay";
}

// Core step rendering logic
function renderStepState() {
  if (currentStepIndex < 0 || currentStepIndex >= simSteps.length) return;
  
  const step = simSteps[currentStepIndex];
  const state = step.state_after;
  
  const algorithm = algorithmSelect.value.toLowerCase();
  
  // 1. Render TLB Grid
  renderTlbDisplay(state.tlb, step);
  
  // 2. Render Frame Table (RAM)
  renderFrameDisplay(state.frame_table, state.clock_hand, algorithm, step);
  
  // 3. Render Page Table
  renderPageTableDisplay(state.page_table, step.vpn, algorithm);

  // 4. Update Stats gauges and cards up to current step
  updateStatsDisplay();

  // 5. Render Math breakdown
  renderMathBreakdown(step);

  // 6. Render Console log
  renderConsoleLogs();
}

// 1. Render TLB Display
function renderTlbDisplay(tlbArray, step) {
  tlbGrid.innerHTML = "";
  if (!tlbArray || tlbArray.length === 0) {
    tlbGrid.innerHTML = '<div class="empty-state">TLB cache is disabled (size = 0)</div>';
    document.getElementById("tlb-badge").className = "badge orange-badge";
    document.getElementById("tlb-badge").textContent = "Disabled";
    return;
  }
  
  document.getElementById("tlb-badge").className = "badge";
  document.getElementById("tlb-badge").textContent = "Enabled";
  
  tlbArray.forEach((entry, idx) => {
    const item = document.createElement("div");
    item.className = "grid-item glass-card";
    if (entry.valid) {
      item.classList.add("occupied");
    }
    
    // Highlight if this entry was accessed/hit in the current step
    const isTarget = entry.valid && entry.vpn === step.vpn;
    if (isTarget) {
      if (step.tlb_hit) {
        item.classList.add("highlight-hit");
      } else if (step.loaded_frame !== null && entry.frame_number === step.loaded_frame) {
        // Just loaded into TLB on miss
        item.classList.add("highlight-miss");
      }
    }
    
    item.innerHTML = `
      <span class="grid-item-title">Entry ${idx}</span>
      <span class="grid-item-val">${entry.valid ? `VPN: ${entry.vpn}` : "INVALID"}</span>
      <span class="grid-item-sub">${entry.valid ? `Frame: ${entry.frame_number}` : "-"}</span>
      <span class="grid-item-sub" style="font-size: 8px; opacity:0.6;">
        ${entry.valid ? `Load: t${entry.load_timestamp} | Use: t${entry.last_used_timestamp}` : ""}
      </span>
    `;
    tlbGrid.appendChild(item);
  });
}

// 2. Render Frame Table Display
function renderFrameDisplay(frameArray, clockHand, algorithm, step) {
  frameGrid.innerHTML = "";
  if (!frameArray || frameArray.length === 0) {
    frameGrid.innerHTML = '<div class="empty-state">No frame structures found</div>';
    return;
  }

  // Display clock hand position if algorithm is clock
  if (algorithm === "clock") {
    clockHandValBadge.classList.remove("hidden");
    clockHandValBadge.textContent = `Clock Hand: Frame ${clockHand}`;
  } else {
    clockHandValBadge.classList.add("hidden");
  }

  frameArray.forEach((frame, idx) => {
    const item = document.createElement("div");
    item.className = "grid-item glass-card";
    if (frame.occupied) {
      item.classList.add("occupied");
    }

    // Highlight frame if it was involved in page fault load / eviction in this step
    const isModified = frame.occupied && (idx === step.loaded_frame || idx === step.evicted_frame);
    if (isModified) {
      if (step.page_fault) {
        item.classList.add("highlight-miss");
      }
    }

    // Clock Hand Indicator Dot
    let clockIndicatorHtml = "";
    if (algorithm === "clock" && idx === clockHand) {
      clockIndicatorHtml = '<div class="clock-hand-indicator" title="Clock Hand points here"></div>';
    }

    item.innerHTML = `
      ${clockIndicatorHtml}
      <span class="grid-item-title">Frame ${idx}</span>
      <span class="grid-item-val">${frame.occupied ? `VPN: ${frame.vpn}` : "FREE"}</span>
      <span class="grid-item-sub">${frame.occupied ? `Load: t${frame.load_timestamp}` : "-"}</span>
    `;
    frameGrid.appendChild(item);
  });
}

// 3. Render Page Table Display
function renderPageTableDisplay(pageTableArray, activeVpn, algorithm) {
  pageTableBody.innerHTML = "";
  if (!pageTableArray || pageTableArray.length === 0) {
    pageTableBody.innerHTML = '<tr><td colspan="6" class="empty-state">No page entries found</td></tr>';
    return;
  }

  pageTableArray.forEach((pte, idx) => {
    const tr = document.createElement("tr");
    if (idx === activeVpn) {
      tr.className = "active-row";
    }

    const presentCell = pte.present 
      ? '<span class="badge">Present</span>' 
      : '<span class="badge orange-badge">Absent</span>';
      
    const dirtyCell = pte.dirty 
      ? '<span class="badge purple-badge">Dirty</span>' 
      : '<span class="text-muted">-</span>';

    const refCell = pte.referenced 
      ? '<span class="badge">1</span>' 
      : '<span class="text-muted">0</span>';

    const lastColVal = algorithm === 'lfu' ? (pte.access_count || 0) : `t${pte.last_used_timestamp}`;

    tr.innerHTML = `
      <td style="font-weight:600; font-family:monospace;">VPN ${idx}</td>
      <td>${presentCell}</td>
      <td style="font-family:monospace;">${pte.present ? `Frame ${pte.frame_number}` : "-"}</td>
      <td>${dirtyCell}</td>
      <td>${refCell}</td>
      <td style="font-family:monospace; color:var(--text-muted);">${lastColVal}</td>
    `;
    pageTableBody.appendChild(tr);
  });

  // Scroll the active row into view if needed
  const activeRow = pageTableBody.querySelector(".active-row");
  if (activeRow) {
    activeRow.scrollIntoView({ behavior: "smooth", block: "nearest" });
  }
}

// 4. Update Stats Displays
function updateStatsDisplay() {
  // We calculate dynamic stats up to the currentStepIndex
  let accesses = 0;
  let reads = 0;
  let writes = 0;
  let tlbHits = 0;
  let tlbMisses = 0;
  let pageFaults = 0;
  let evictions = 0;
  let dirtyWritebacks = 0;

  for (let i = 0; i <= currentStepIndex; i++) {
    const step = simSteps[i];
    accesses++;
    if (step.op === "READ") reads++;
    else if (step.op === "WRITE") writes++;
    
    if (step.tlb_hit) tlbHits++;
    else tlbMisses++;
    
    if (step.page_fault) pageFaults++;
    if (step.evicted_frame !== null && step.evicted_vpn !== null) evictions++;
    if (step.dirty_writeback || step.dirty_write_back) dirtyWritebacks++;
  }

  statAccesses.textContent = accesses;
  statRW.textContent = `${reads} R / ${writes} W`;
  statTlbHits.textContent = tlbHits;
  statTlbMisses.textContent = tlbMisses;
  statPageFaults.textContent = pageFaults;
  statEvictions.textContent = evictions;
  statDirty.textContent = dirtyWritebacks;

  // Gauge hit/fault rates
  const hitRate = accesses > 0 ? (tlbHits / accesses) * 100 : 0;
  const faultRate = accesses > 0 ? (pageFaults / accesses) * 100 : 0;

  tlbGaugeTxt.textContent = `${hitRate.toFixed(1)}%`;
  pfGaugeTxt.textContent = `${faultRate.toFixed(1)}%`;

  // Update radial circle offset: stroke-dashoffset = circumference * (1 - pct/100)
  // circumference is 251.2
  const c = 251.2;
  tlbGaugeFill.style.strokeDashoffset = c * (1 - hitRate / 100);
  pfGaugeFill.style.strokeDashoffset = c * (1 - faultRate / 100);
}

// 5. Render Address Translation Breakdown Math
function renderMathBreakdown(step) {
  const pageSize = parseInt(pageSizeSelect.value, 10);
  
  // Calculate offset bits: log2(pageSize)
  const offsetBits = Math.log2(pageSize);
  
  const vaInt = parseInt(step.va, 16);
  const vaBin = vaInt.toString(2).padStart(16, "0");
  
  const vpnBits = 16 - offsetBits;
  const vpnBinPart = vaBin.slice(0, vpnBits);
  const offsetBinPart = vaBin.slice(vpnBits);
  
  const outcomeText = step.tlb_hit 
    ? `<span class="tlb-hit">TLB Hit</span>. Translation resolved in TLB.` 
    : step.page_fault
      ? `<span class="page-fault">TLB Miss -> Page Fault</span>. Loaded from Disk.`
      : `<span class="text-muted">TLB Miss -> Page Table Hit</span>. Resolved in Page Table.`;

  const evictText = step.evicted_vpn !== null
    ? `\n<span class="dirty-write">Eviction required</span>: Evict VPN ${step.evicted_vpn} from Frame ${step.evicted_frame}.${step.dirty_writeback ? " (Requires dirty write-back to disk)" : ""}`
    : "";

  mathBreakdownPanel.innerHTML = `
<div class="math-item"><span>Virtual Address:</span><strong>${step.va}</strong></div>
<div class="math-item"><span>Decimal VA:</span><strong>${vaInt}</strong></div>
<div class="math-item"><span>Binary breakdown:</span><strong style="color:var(--accent-cyan);">${vpnBinPart}</strong><strong style="color:var(--accent-purple);">${offsetBinPart}</strong></div>
<div style="font-size:10px; color:var(--text-muted); display:flex; justify-content:space-between; margin-bottom:8px;">
  <span>VPN (${vpnBits} bits)</span>
  <span>Offset (${offsetBits} bits)</span>
</div>

<div class="math-line"></div>
<div class="math-item"><span>VPN:</span><strong>VA / ${pageSize} = ${step.vpn}</strong></div>
<div class="math-item"><span>Offset:</span><strong>VA % ${pageSize} = ${step.offset}</strong></div>

<div class="math-line"></div>
<div class="math-item"><span>Outcome:</span><strong>${outcomeText}${evictText}</strong></div>
<div class="math-item"><span>Physical Frame:</span><strong>Frame ${step.loaded_frame !== null ? step.loaded_frame : step.state_after.page_table[step.vpn].frame_number}</strong></div>
<div class="math-item"><span>Physical Address:</span><strong class="tlb-hit">${step.pa}</strong></div>
  `;
}

// 6. Render Console Log
function renderConsoleLogs() {
  consoleLogsContainer.innerHTML = "";
  
  for (let i = 0; i <= currentStepIndex; i++) {
    const step = simSteps[i];
    
    // Step Header
    const header = document.createElement("div");
    header.className = "console-line console-step-header";
    header.textContent = `[${String(i).padStart(2, "0")}] ${step.op} VA=${step.va} VPN=${step.vpn} OFFSET=${step.offset}`;
    consoleLogsContainer.appendChild(header);

    // Indented Event Line 1
    const line1 = document.createElement("div");
    line1.className = "console-line console-log-item";
    if (step.tlb_hit) {
      line1.innerHTML = `TLB hit -> PA=<span class="tlb-hit">${step.pa}</span>`;
    } else {
      if (step.page_fault) {
        if (step.evicted_vpn !== null) {
          line1.innerHTML = `TLB miss -> page fault -> RAM full`;
          
          // Evict line
          const evictLine = document.createElement("div");
          evictLine.className = "console-line console-log-item";
          if (step.dirty_writeback || step.dirty_write_back) {
            evictLine.innerHTML = `evict VPN=${step.evicted_vpn} from FRAME=${step.evicted_frame} -> <span class="dirty-write">dirty write-back</span>`;
          } else {
            evictLine.innerHTML = `evict VPN=${step.evicted_vpn} from FRAME=${step.evicted_frame}`;
          }
          consoleLogsContainer.appendChild(evictLine);
        } else {
          line1.innerHTML = `TLB miss -> page fault`;
        }
        
        // Load line
        const loadLine = document.createElement("div");
        loadLine.className = "console-line console-log-item";
        loadLine.innerHTML = `load VPN=${step.vpn} into FRAME=${step.loaded_frame} -> PA=<span class="tlb-hit">${step.pa}</span>`;
        consoleLogsContainer.appendChild(loadLine);
      } else {
        line1.innerHTML = `TLB miss -> PA=<span class="tlb-hit">${step.pa}</span>`;
      }
    }
    
    if (step.tlb_hit || !step.page_fault) {
      consoleLogsContainer.appendChild(line1);
    }
  }

  // Auto-scroll console to bottom
  consoleLogsContainer.scrollTop = consoleLogsContainer.scrollHeight;
}
