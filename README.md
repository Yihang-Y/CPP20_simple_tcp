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

### Benchmark Tools

1. **Python Benchmark Tool** (`benchmark.py`)
   - 使用 Rust 基准测试工具进行性能测试
   - 支持多客户端并发测试
   - 生成性能对比图表

2. **Simple Test Tool** (`test_coroutine_echo.py`)
   - 轻量级测试工具，用于基本功能验证
   - 支持自定义并发客户端数量
   - 支持自定义消息大小
   - 提供详细的性能指标

## Building and Running

```bash
# Build Coroutine Echo Server
cd coroutine_echo && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build

# Build Epoll Echo Server
cd epoll_echo && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build

# Run Comprehensive Benchmarks
python benchmark.py

# Run Simple Test
python test_coroutine_echo.py --clients 100 --message-length 512 --messages 1000
```

## Requirements
- C++20 compatible compiler
- Linux kernel 5.1+ (for io_uring support)
- CMake 3.15+
- Python 3.6+ (for benchmarking)
- Rust (for benchmark tool)

## Results
Performance comparison results are available in the results/ directory.

## License
MIT
