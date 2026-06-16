# Virtual Memory Management Simulator

A C11 command-line simulator modeling the fundamental concepts of operating system virtual memory management. The simulator performs virtual-to-physical address translation, detects page faults, implements page replacement algorithms (FIFO, LRU, Clock), models a Translation Lookaside Buffer (TLB), and tracks detailed statistics.

## Project Structure

- `src/` - Core C source code for the simulator engine, replacement algorithms, TLB, and stats.
- `include/` - Header files defining the core data structures and module interfaces.
- `scripts/` - Scripts for generating benchmark trace patterns and running the benchmarking suite (`benchmark.sh`).
- `results/` - CSV output of the benchmarking results.

## Build Instructions

The project includes a `Makefile`. To compile the simulator, simply run:

```bash
make
# Or on Windows: mingw32-make
```

To clean the compiled object files and executables:
```bash
make clean
```

## Usage

The simulator is executed from the command line using the following syntax:

```bash
./vmsim --pages <count> --frames <count> --page-size <bytes> --tlb-size <count> --algorithm <fifo|lru|clock> <trace_file>
```

**Example:**
```bash
./vmsim --pages 8 --frames 3 --page-size 4096 --tlb-size 2 --algorithm clock tests/traces/trace_basic.txt
```

### Parameters
- `--pages`: The total number of virtual pages in the system.
- `--frames`: The number of physical memory frames available (must be > 0).
- `--page-size`: The size of each page in bytes (must be a power of two).
- `--tlb-size`: The maximum number of entries the TLB cache can hold (0 to disable TLB).
- `--algorithm`: The page replacement algorithm to use when physical RAM is full (`fifo`, `lru`, or `clock`).
- `<trace_file>`: Path to a text file containing memory operations (e.g., `R 0x1000` or `W 0x0FFF`).

## Benchmarking

To benchmark the simulator across various configurations (algorithms, frame counts, TLB sizes) and trace patterns, run:

```bash
python scripts/generate_traces.py
bash scripts/benchmark.sh
```
This will output the comparison statistics into `results/benchmark_results.csv`.
