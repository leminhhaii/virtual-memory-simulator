#!/usr/bin/env bash

# Define paths
EXE="./vmsim"
if [ -f "./vmsim.exe" ]; then
    EXE="./vmsim.exe"
fi

CSV_OUT="results/benchmark_results.csv"

# Write CSV header
echo "Algorithm,Frames,TLB_Size,Trace,Total_Accesses,Total_Reads,Total_Writes,TLB_Hits,TLB_Misses,Page_Faults,Evictions,Dirty_Write_Backs,TLB_Hit_Rate,Page_Fault_Rate" > "$CSV_OUT"

ALGOS=("fifo" "lru" "clock" "lfu")
FRAMES=(2 3 5 8)
TLB_SIZES=(0 2 4 8)
TRACES=("bench_seq" "bench_loc" "bench_mix" "bench_thr")

for algo in "${ALGOS[@]}"; do
    for frames in "${FRAMES[@]}"; do
        for tlb in "${TLB_SIZES[@]}"; do
            for trace_name in "${TRACES[@]}"; do
                trace_file="tests/traces/${trace_name}.txt"
                
                # Run simulator
                OUT_FILE="tests/temp_bench_out.txt"
                $EXE --pages 16 --frames $frames --page-size 4096 --tlb-size $tlb --algorithm $algo $trace_file > "$OUT_FILE" 2>/dev/null
                
                # Parse stats
                acc=$(grep -i "^Total Accesses:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                rd=$(grep -i "^Total Reads:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                wr=$(grep -i "^Total Writes:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                t_hit=$(grep -i "^TLB Hits:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                t_miss=$(grep -i "^TLB Misses:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                pf=$(grep -i "^Page Faults:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                ev=$(grep -i "^Evictions:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                dw=$(grep -i "^Dirty Write-backs:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | xargs)
                t_rate=$(grep -i "^TLB Hit Rate:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | tr -d '%' | xargs)
                p_rate=$(grep -i "^Page Fault Rate:" "$OUT_FILE" | awk -F': ' '{print $2}' | tr -d '\r' | tr -d '%' | xargs)
                
                echo "$algo,$frames,$tlb,$trace_name,$acc,$rd,$wr,$t_hit,$t_miss,$pf,$ev,$dw,$t_rate,$p_rate" >> "$CSV_OUT"
            done
        done
    done
done

rm -f tests/temp_bench_out.txt
echo "Benchmarking complete. Results saved to $CSV_OUT"
