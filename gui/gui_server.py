import os
import re
import copy
import sys
import uuid
import platform
import subprocess
from typing import List, Dict, Any, Optional
from fastapi import FastAPI, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import RedirectResponse
from pydantic import BaseModel, Field

# Define FastAPI application
app = FastAPI(title="Virtual Memory Simulator Web UI")

# Base directories
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
WORKSPACE_DIR = os.path.abspath(os.path.join(CURRENT_DIR, ".."))
TRACES_DIR = os.path.join(WORKSPACE_DIR, "tests", "traces")

# State Reconstructor
class VMStateReconstructor:
    def __init__(self, pages: int, frames: int, page_size: int, tlb_size: int, algorithm: str):
        self.pages = pages
        self.frames = frames
        self.page_size = page_size
        self.tlb_size = tlb_size
        self.algorithm = algorithm.lower()
        
        self.logical_clock = 0
        self.clock_hand = 0
        
        self.tlb: List[Dict[str, Any]] = []
        if self.tlb_size > 0:
            self.tlb = [{
                "valid": False, "vpn": 0, "frame_number": -1, 
                "load_timestamp": 0, "last_used_timestamp": 0
            } for _ in range(self.tlb_size)]
            
        self.page_table = [{
            "present": False, "frame_number": -1, "dirty": False, 
            "referenced": False, "last_used_timestamp": 0, "access_count": 0
        } for _ in range(self.pages)]
        
        self.frame_table = [{
            "occupied": False, "vpn": 0, "load_timestamp": 0
        } for _ in range(self.frames)]
        
        self.stats = {
            "total_accesses": 0, "total_reads": 0, "total_writes": 0,
            "tlb_hits": 0, "tlb_misses": 0, "page_faults": 0,
            "evictions": 0, "dirty_write_backs": 0
        }

    def tlb_insert(self, vpn: int, frame_num: int):
        if self.tlb_size == 0:
            return
            
        # Update if already exists
        for entry in self.tlb:
            if entry["valid"] and entry["vpn"] == vpn:
                entry["frame_number"] = frame_num
                entry["last_used_timestamp"] = self.logical_clock
                return
                
        # Find invalid slot
        for entry in self.tlb:
            if not entry["valid"]:
                entry["valid"] = True
                entry["vpn"] = vpn
                entry["frame_number"] = frame_num
                entry["load_timestamp"] = self.logical_clock
                entry["last_used_timestamp"] = self.logical_clock
                return
                
        # Evict & replace
        victim_idx = 0
        if self.algorithm == "lru":
            min_time = min(e["last_used_timestamp"] for e in self.tlb)
            victim_idx = next(i for i, e in enumerate(self.tlb) if e["last_used_timestamp"] == min_time)
        else:
            min_time = min(e["load_timestamp"] for e in self.tlb)
            victim_idx = next(i for i, e in enumerate(self.tlb) if e["load_timestamp"] == min_time)
            
        self.tlb[victim_idx] = {
            "valid": True, "vpn": vpn, "frame_number": frame_num,
            "load_timestamp": self.logical_clock, "last_used_timestamp": self.logical_clock
        }

    def tlb_invalidate(self, vpn: int):
        for entry in self.tlb:
            if entry["valid"] and entry["vpn"] == vpn:
                entry["valid"] = False
                break

    def choose_victim_frame(self) -> int:
        if self.algorithm == "fifo":
            min_time = min(f["load_timestamp"] for f in self.frame_table if f["occupied"])
            return next(i for i, f in enumerate(self.frame_table) if f["occupied"] and f["load_timestamp"] == min_time)
            
        elif self.algorithm == "lru":
            min_time = float('inf')
            victim = -1
            for idx, frame in enumerate(self.frame_table):
                if frame["occupied"]:
                    vpn = frame["vpn"]
                    last_used = self.page_table[vpn]["last_used_timestamp"]
                    if last_used < min_time:
                        min_time = last_used
                        victim = idx
            return victim
            
        elif self.algorithm == "clock":
            while True:
                curr = self.clock_hand
                self.clock_hand = (self.clock_hand + 1) % self.frames
                if self.frame_table[curr]["occupied"]:
                    vpn = self.frame_table[curr]["vpn"]
                    if self.page_table[vpn]["referenced"]:
                        self.page_table[vpn]["referenced"] = False
                    else:
                        return curr
                        
        elif self.algorithm == "lfu":
            min_access = float('inf')
            min_load_time = float('inf')
            victim = -1
            for idx, frame in enumerate(self.frame_table):
                if frame["occupied"]:
                    vpn = frame["vpn"]
                    access_cnt = self.page_table[vpn]["access_count"]
                    load_time = frame["load_timestamp"]
                    if access_cnt < min_access:
                        min_access = access_cnt
                        min_load_time = load_time
                        victim = idx
                    elif access_cnt == min_access:
                        if load_time < min_load_time:
                            min_load_time = load_time
                            victim = idx
            return victim
        return -1

    def access_address(self, op: str, addr: int) -> Dict[str, Any]:
        vpn = addr // self.page_size
        offset = addr % self.page_size
        
        state_before = {
            "logical_clock": self.logical_clock,
            "clock_hand": self.clock_hand,
            "tlb": copy.deepcopy(self.tlb),
            "page_table": copy.deepcopy(self.page_table),
            "frame_table": copy.deepcopy(self.frame_table)
        }
        
        # Stats increment
        self.stats["total_accesses"] += 1
        if op == 'R':
            self.stats["total_reads"] += 1
        else:
            self.stats["total_writes"] += 1
            
        tlb_hit = False
        page_fault = False
        ram_full = False
        evicted_vpn = None
        evicted_frame = None
        dirty_write_back = False
        frame_num = -1
        
        # TLB lookup
        hit_in_tlb = False
        if self.tlb_size > 0:
            for entry in self.tlb:
                if entry["valid"] and entry["vpn"] == vpn:
                    hit_in_tlb = True
                    entry["last_used_timestamp"] = self.logical_clock
                    frame_num = entry["frame_number"]
                    break
            if hit_in_tlb:
                tlb_hit = True
                self.stats["tlb_hits"] += 1
            else:
                self.stats["tlb_misses"] += 1
        else:
            self.stats["tlb_misses"] += 1
            
        if not hit_in_tlb:
            pte = self.page_table[vpn]
            if pte["present"]:
                frame_num = pte["frame_number"]
                self.tlb_insert(vpn, frame_num)
            else:
                page_fault = True
                self.stats["page_faults"] += 1
                
                free_frame = next((i for i, f in enumerate(self.frame_table) if not f["occupied"]), -1)
                if free_frame != -1:
                    frame_num = free_frame
                    self.frame_table[frame_num] = {"occupied": True, "vpn": vpn, "load_timestamp": self.logical_clock}
                    pte.update({"present": True, "frame_number": frame_num, "dirty": False, "referenced": False, "access_count": 0})
                    self.tlb_insert(vpn, frame_num)
                else:
                    ram_full = True
                    victim = self.choose_victim_frame()
                    evicted_vpn = self.frame_table[victim]["vpn"]
                    ev_pte = self.page_table[evicted_vpn]
                    evicted_frame = victim
                    self.stats["evictions"] += 1
                    
                    if ev_pte["dirty"]:
                        dirty_write_back = True
                        self.stats["dirty_write_backs"] += 1
                        
                    self.tlb_invalidate(evicted_vpn)
                    ev_pte.update({"present": False, "dirty": False, "referenced": False, "access_count": 0})
                    
                    self.frame_table[victim] = {"occupied": True, "vpn": vpn, "load_timestamp": self.logical_clock}
                    pte.update({"present": True, "frame_number": victim, "dirty": False, "referenced": False, "access_count": 0})
                    self.tlb_insert(vpn, victim)
                    frame_num = victim
                    
        phys_addr = frame_num * self.page_size + offset
        
        pte = self.page_table[vpn]
        pte["last_used_timestamp"] = self.logical_clock
        pte["referenced"] = True
        pte["access_count"] += 1
        if op == 'W':
            pte["dirty"] = True
            
        self.logical_clock += 1
        
        state_after = {
            "logical_clock": self.logical_clock,
            "clock_hand": self.clock_hand,
            "tlb": copy.deepcopy(self.tlb),
            "page_table": copy.deepcopy(self.page_table),
            "frame_table": copy.deepcopy(self.frame_table)
        }
        
        return {
            "op": op, "va": f"0x{addr:04X}", "vpn": vpn, "offset": offset,
            "tlb_hit": tlb_hit, "tlb_miss": not tlb_hit, "page_fault": page_fault, "ram_full": ram_full,
            "evicted_vpn": evicted_vpn, "evicted_frame": evicted_frame, "dirty_write_back": dirty_write_back,
            "loaded_vpn": vpn if page_fault else None, "loaded_frame": frame_num if page_fault else None,
            "pa": f"0x{phys_addr:04X}", "state_before": state_before, "state_after": state_after
        }


# Request and Response schemas
class SimulateRequest(BaseModel):
    pages: int = Field(default=16, gt=0)
    frames: int = Field(default=4, gt=0)
    page_size: int = Field(default=4096, gt=0)
    tlb_size: int = Field(default=4, ge=0)
    algorithm: str = Field(default="fifo")
    trace_file: Optional[str] = None
    custom_trace: Optional[str] = None

# Regex patterns for stdout parser
ACCESS_RE = re.compile(
    r"^\[(?P<step_index>\d+)\]\s+(?P<op>READ|WRITE)\s+VA=(?P<va>0x[0-9A-Fa-f]+)\s+VPN=(?P<vpn>\d+)\s+OFFSET=(?P<offset>\d+)"
)
TLB_HIT_RE = re.compile(r"^\s+TLB hit -> PA=(?P<pa>0x[0-9A-Fa-f]+)")
TLB_MISS_PF_RE = re.compile(r"^\s+TLB miss -> page fault$")
TLB_MISS_PF_FULL_RE = re.compile(r"^\s+TLB miss -> page fault -> RAM full$")
TLB_MISS_PA_RE = re.compile(r"^\s+TLB miss -> PA=(?P<pa>0x[0-9A-Fa-f]+)")
EVICT_RE = re.compile(r"^\s+evict VPN=(?P<evict_vpn>\d+) from FRAME=(?P<evict_frame>\d+)$")
EVICT_DIRTY_RE = re.compile(r"^\s+evict VPN=(?P<evict_vpn>\d+) from FRAME=(?P<evict_frame>\d+) -> dirty write-back$")
LOAD_RE = re.compile(r"^\s+load VPN=(?P<load_vpn>\d+) into FRAME=(?P<load_frame>\d+) -> PA=(?P<pa>0x[0-9A-Fa-f]+)")


@app.get("/api/traces")
def get_traces():
    """Scan tests/traces directory and return available trace files."""
    if not os.path.exists(TRACES_DIR):
        return []
    try:
        files = [f for f in os.listdir(TRACES_DIR) if os.path.isfile(os.path.join(TRACES_DIR, f))]
        # Sort files alphabetically
        return sorted(files)
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/api/simulate")
def simulate(req: SimulateRequest):
    """Invoke the simulator executable and reconstruct the state sequence."""
    # Validate request: exactly one of trace_file or custom_trace must be provided
    if not req.trace_file and not req.custom_trace:
        raise HTTPException(status_code=400, detail="Either 'trace_file' or 'custom_trace' must be provided.")
    if req.trace_file and req.custom_trace:
        raise HTTPException(status_code=400, detail="Cannot provide both 'trace_file' and 'custom_trace'.")

    # Validate custom trace limits
    if req.custom_trace:
        if len(req.custom_trace) > 50000 or len(req.custom_trace.splitlines()) > 1000:
            raise HTTPException(status_code=400, detail="Custom trace exceeds limits (max 1000 lines or 50KB).")

    # Determine trace path
    target_trace_path = ""
    temp_file_created = False
    
    if req.trace_file:
        target_trace_path = os.path.abspath(os.path.join(TRACES_DIR, req.trace_file))
        # Directory traversal protection
        try:
            is_valid = os.path.commonpath([TRACES_DIR]) == os.path.commonpath([TRACES_DIR, target_trace_path])
        except ValueError:
            is_valid = False
        if not is_valid:
            raise HTTPException(status_code=400, detail="Invalid trace file name.")
        if not os.path.exists(target_trace_path):
            raise HTTPException(status_code=400, detail=f"Trace file '{req.trace_file}' not found.")
    else:
        # Write custom_trace to a temporary file
        temp_filename = f"temp_custom_trace_{uuid.uuid4().hex}.txt"
        target_trace_path = os.path.abspath(os.path.join(CURRENT_DIR, temp_filename))
        temp_file_created = True
        try:
            with open(target_trace_path, "w", encoding="utf-8") as f:
                f.write(req.custom_trace)
        except Exception as e:
            raise HTTPException(status_code=500, detail=f"Failed to create temporary trace file: {e}")

    # Determine compiler binary location
    is_windows = platform.system() == "Windows" or sys.platform.startswith("win")
    bin_name = "vmsim.exe" if is_windows else "vmsim"
    bin_path = os.path.abspath(os.path.join(WORKSPACE_DIR, bin_name))

    if not os.path.exists(bin_path):
        # Clean up temp file if needed
        if temp_file_created and os.path.exists(target_trace_path):
            os.remove(target_trace_path)
        raise HTTPException(status_code=500, detail=f"Simulator binary not found at '{bin_path}'. Please compile it first.")

    # Execute simulation
    cmd = [
        bin_path,
        "--pages", str(req.pages),
        "--frames", str(req.frames),
        "--page-size", str(req.page_size),
        "--tlb-size", str(req.tlb_size),
        "--algorithm", req.algorithm,
        target_trace_path
    ]

    try:
        # Run subprocess
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to run simulator: {e}")
    finally:
        # Ensure temporary file is deleted
        if temp_file_created and os.path.exists(target_trace_path):
            try:
                os.remove(target_trace_path)
            except Exception:
                pass

    # Check for error outputs from the simulator or non-zero exit codes
    if result.returncode != 0:
        err_msg = (result.stderr or result.stdout or f"Simulator failed with exit code {result.returncode}").strip()
        # Extract syntax error message
        raise HTTPException(status_code=400, detail=err_msg)

    # Parse stdout line-by-line
    lines = result.stdout.splitlines()
    steps = []
    curr_step = None

    for line in lines:
        m_access = ACCESS_RE.match(line)
        if m_access:
            curr_step = {
                "clock": len(steps),
                "step_index": int(m_access.group("step_index")),
                "op": m_access.group("op"),
                "va": m_access.group("va"),
                "vpn": int(m_access.group("vpn")),
                "offset": int(m_access.group("offset")),
                "tlb": "miss",
                "tlb_hit": False,
                "tlb_miss": True,
                "fault": False,
                "page_fault": False,
                "ram_full": False,
                "evicted_vpn": None,
                "evicted_frame": None,
                "dirty_writeback": False,
                "dirty_write_back": False,
                "loaded_vpn": None,
                "loaded_frame": None,
                "pa": ""
            }
            steps.append(curr_step)
            continue

        if curr_step is not None:
            m_tlb_hit = TLB_HIT_RE.match(line)
            if m_tlb_hit:
                curr_step["tlb"] = "hit"
                curr_step["tlb_hit"] = True
                curr_step["tlb_miss"] = False
                curr_step["pa"] = m_tlb_hit.group("pa")
                continue

            m_tlb_miss_pa = TLB_MISS_PA_RE.match(line)
            if m_tlb_miss_pa:
                curr_step["tlb"] = "miss"
                curr_step["tlb_hit"] = False
                curr_step["tlb_miss"] = True
                curr_step["pa"] = m_tlb_miss_pa.group("pa")
                continue

            m_pf = TLB_MISS_PF_RE.match(line)
            if m_pf:
                curr_step["fault"] = True
                curr_step["page_fault"] = True
                continue

            m_pf_full = TLB_MISS_PF_FULL_RE.match(line)
            if m_pf_full:
                curr_step["fault"] = True
                curr_step["page_fault"] = True
                curr_step["ram_full"] = True
                continue

            m_evict = EVICT_RE.match(line)
            if m_evict:
                curr_step["evicted_vpn"] = int(m_evict.group("evict_vpn"))
                curr_step["evicted_frame"] = int(m_evict.group("evict_frame"))
                continue

            m_evict_dirty = EVICT_DIRTY_RE.match(line)
            if m_evict_dirty:
                curr_step["evicted_vpn"] = int(m_evict_dirty.group("evict_vpn"))
                curr_step["evicted_frame"] = int(m_evict_dirty.group("evict_frame"))
                curr_step["dirty_writeback"] = True
                curr_step["dirty_write_back"] = True
                continue

            m_load = LOAD_RE.match(line)
            if m_load:
                curr_step["loaded_vpn"] = int(m_load.group("load_vpn"))
                curr_step["loaded_frame"] = int(m_load.group("load_frame"))
                curr_step["pa"] = m_load.group("pa")
                continue

    # Reconstruct state machine step-by-step
    reconstructor = VMStateReconstructor(
        pages=req.pages,
        frames=req.frames,
        page_size=req.page_size,
        tlb_size=req.tlb_size,
        algorithm=req.algorithm
    )

    for step in steps:
        addr_int = int(step["va"], 16)
        op_char = 'R' if step["op"] == "READ" else 'W'
        reconstructed = reconstructor.access_address(op_char, addr_int)
        
        # Inject state information into the step payload
        step["state_before"] = reconstructed["state_before"]
        step["state_after"] = reconstructed["state_after"]

    # Calculate statistics matching both PROJECT.md format and explorer schemas
    total_acc = reconstructor.stats["total_accesses"]
    tlb_hit_rate = (reconstructor.stats["tlb_hits"] / total_acc * 100.0) if total_acc > 0 else 0.0
    page_fault_rate = (reconstructor.stats["page_faults"] / total_acc * 100.0) if total_acc > 0 else 0.0

    stats_payload = {
        "accesses": total_acc,
        "reads": reconstructor.stats["total_reads"],
        "writes": reconstructor.stats["total_writes"],
        "tlb_hits": reconstructor.stats["tlb_hits"],
        "tlb_misses": reconstructor.stats["tlb_misses"],
        "page_faults": reconstructor.stats["page_faults"],
        "evictions": reconstructor.stats["evictions"],
        "dirty_writebacks": reconstructor.stats["dirty_write_backs"],
        "tlb_hit_rate": round(tlb_hit_rate, 2),
        "page_fault_rate": round(page_fault_rate, 2),
        
        # Explorer schema duplicates
        "total_accesses": total_acc,
        "total_reads": reconstructor.stats["total_reads"],
        "total_writes": reconstructor.stats["total_writes"],
        "dirty_write_backs": reconstructor.stats["dirty_write_backs"]
    }

    return {
        "success": True,
        "error": None,
        "steps": steps,
        "statistics": stats_payload,
        "stats": stats_payload  # Compatible alias
    }


# Route home to static files index.html
@app.get("/")
def get_root():
    return RedirectResponse(url="/static/index.html")

# Mount Static Files (must be loaded after explicit routes to prevent shadowing)
static_path = os.path.join(CURRENT_DIR, "static")
if not os.path.exists(static_path):
    os.makedirs(static_path)
app.mount("/static", StaticFiles(directory=static_path), name="static")
