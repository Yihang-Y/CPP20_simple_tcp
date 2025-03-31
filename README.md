# C++20 TCP Echo Server Implementations

A performance comparison of different TCP echo server implementations
## Implementations

### 1. Coroutine Echo Server (coroutine_echo/)
- Built with C++20 coroutines and Linux io_uring
- Single-threaded asynchronous I/O scheduler
- Features:
  - Task/Promise-based coroutine lifecycle management
  - io_uring-based zero-copy I/O operations
  - Supports concurrent operations with co_spawn

### 2. Epoll Echo Server (epoll_echo/)
- Traditional event-driven implementation using epoll
- Callback-based asynchronous I/O
- Features:
  - Event loop with epoll
  - Non-blocking I/O operations

## Performance Benchmarks

The project includes comprehensive benchmarking tools that measure:
- Throughput (requests/second)
- Connection handling capacity
- Performance with different message sizes (16B, 2KB, etc.)

## Building and Running

```bash
# Build Coroutine Echo Server
cd coroutine_echo && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build

# Build Epoll Echo Server
cd epoll_echo && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build

# Run Benchmarks
python benchmark.py
```

## Requirements
- C++20 compatible compiler
- Linux kernel 5.1+ (for io_uring support)
- CMake 3.15+
- Python 3.6+ (for benchmarking)
- Rust (for benchmark tool)

## Results
Performance comparison results are available in the benchmark_results_*.json files and corresponding PNG visualizations.