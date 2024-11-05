# System Monitor

This C++ program provides real-time monitoring of system resources, including:

- **CPU Usage:** Overall CPU usage and per-core usage.
- **Memory Usage:** Total memory used.
- **Disk Usage:** Used disk space.
- **Network Usage:** Total network data sent and received.
- **CPU Temperature:** Current CPU temperature.

## Requirements

- macOS (The code uses macOS-specific APIs)
- C++11 or later compiler

## Compilation

You can compile the code using a C++ compiler like g++:

```bash
g++ -std=c++11 monitor.cpp -o monitor
