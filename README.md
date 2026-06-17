# Virtual Memory Management Simulator

A C11 command-line simulator modeling the fundamental concepts of operating system virtual memory management. The simulator performs virtual-to-physical address translation, detects page faults, implements page replacement algorithms (FIFO, LRU, Clock, LFU), models a Translation Lookaside Buffer (TLB), and tracks detailed statistics. It also includes a modern, web-based graphical user interface (Web UI) for interactive step-by-step execution and visual representation.

## Project Structure

- `src/` - Core C source code for the simulator engine, replacement algorithms, TLB, and stats.
- `include/` - Header files defining the core data structures and module interfaces.
- `gui/` - FastAPI backend server (`gui_server.py`) and Web UI frontend (`static/`).
- `scripts/` - Scripts for generating benchmark trace patterns and running the benchmarking suite (`benchmark.sh`).
- `results/` - CSV output of the benchmarking results.

## Build Instructions (C Simulator)

The project includes a `Makefile`. To compile the simulator, simply run:

```bash
make
# Or on Windows: mingw32-make
```

To clean the compiled object files and executables:
```bash
make clean
```

## CLI Usage

The simulator is executed from the command line using the following syntax:

```bash
./vmsim --pages <count> --frames <count> --page-size <bytes> --tlb-size <count> --algorithm <fifo|lru|clock|lfu> <trace_file>
```

**Example:**
```bash
./vmsim --pages 8 --frames 3 --page-size 4096 --tlb-size 2 --algorithm lfu tests/traces/trace_basic.txt
```

### Parameters
- `--pages`: The total number of virtual pages in the system.
- `--frames`: The number of physical memory frames available (must be > 0).
- `--page-size`: The size of each page in bytes (must be a power of two).
- `--tlb-size`: The maximum number of entries the TLB cache can hold (0 to disable TLB).
- `--algorithm`: The page replacement algorithm to use when physical RAM is full (`fifo`, `lru`, `clock`, or `lfu`).
- `<trace_file>`: Path to a text file containing memory operations (e.g., `R 0x1000` or `W 0x0FFF`).

## Web Graphical User Interface (Web UI)

The project features a sleek, responsive Web UI dashboard built with FastAPI (backend) and Vanilla HTML5/CSS3/JavaScript (frontend) that allows visual step-by-step execution.

### Prerequisites

You need Python 3.7+ installed. Install the required backend dependencies:

```bash
pip install fastapi uvicorn
```

### Running the Web UI

You can launch the Web UI server using the provided scripts or manually:

#### A. Using Helper Scripts (Recommended)
- **Windows**: Double-click `gui/run_gui.bat` (or run it via cmd/PowerShell: `.\gui\run_gui.bat`).
- **macOS / Linux**: Open a terminal and run:
  ```bash
  chmod +x gui/run_gui.sh
  ./gui/run_gui.sh
  ```
  *These scripts will automatically launch the server and open your default web browser.*

#### B. Running Manually
Alternatively, navigate to the `gui` folder and run `uvicorn`:
```bash
cd gui
uvicorn gui_server:app --host 127.0.0.1 --port 8000 --reload
```

Once started, open your browser and navigate to:
```
http://localhost:8000
```

## Benchmarking

To benchmark the simulator across various configurations (algorithms, frame counts, TLB sizes) and trace patterns, run:

```bash
python scripts/generate_traces.py
bash scripts/benchmark.sh
```
This will output the comparison statistics into `results/benchmark_results.csv`.
