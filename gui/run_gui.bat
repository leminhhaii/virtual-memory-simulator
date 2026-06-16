@echo off
echo Starting Virtual Memory Simulator Web UI...
start "" "http://localhost:8000"
uvicorn gui_server:app --host 127.0.0.1 --port 8000
