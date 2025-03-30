#!/bin/bash

OUTPUT_DIR=".perf_test"
SERVER="./build/simple_tcp"
SERVER_STARTUP_DELAY=1
BENCH_TOOL_PATH="../rust_echo_bench"
BENCH_TOOL="cargo run --release -- --address '127.0.0.1:8080' --number 100 --duration 30 --length 512"
RUN_DURATION=10  # 运行时间
FLAMEGRAPH_DIR="/home/tools/FlameGraph"


mkdir -p "$OUTPUT_DIR"

echo "启动 server (在 Valgrind Massif 下)..."
# 立即捕获后台进程的 PID
heaptrack $SERVER > "$OUTPUT_DIR/server.log" 2>&1 &
SERVER_PID=$!

# 启动基准测试
echo "开始基准测试..."
bash -c "cd '$BENCH_TOOL_PATH' && $BENCH_TOOL" > "$OUTPUT_DIR/bench.log" 2>&1 &
BENCH_PID=$!
sleep 1

# 等待一段时间，让 server 运行，并采集足够的内存数据
sleep $RUN_DURATION

# 结束 server (如果需要手动结束)
kill $SERVER_PID

# 等待 server 真正退出，确保 massif.out 文件完整写入
wait $SERVER_PID

# 现在生成内存报告
if [ -d "$FLAMEGRAPH_DIR" ]; then
    heaptrack_print --export-flamegraph heaptrack.gz > "$OUTPUT_DIR/folded.txt"
    $FLAMEGRAPH_DIR/flamegraph.pl "$OUTPUT_DIR/folded.txt" > "$OUTPUT_DIR/heap_flamegraph.svg"
fi
echo "内存分析完成，报告保存在 $OUTPUT_DIR/memory_report.txt"
