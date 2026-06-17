#!/bin/bash
cd "$(dirname "$0")"
echo "Starting Virtual Memory Simulator Web UI..."

# Attempt to open browser in background
if command -v xdg-open > /dev/null; then
  xdg-open "http://localhost:8000" &
elif command -v open > /dev/null; then
  open "http://localhost:8000" &
elif command -v sensible-browser > /dev/null; then
  sensible-browser "http://localhost:8000" &
fi

# Run the FastAPI server
uvicorn gui_server:app --host 127.0.0.1 --port 8000
