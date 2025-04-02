#!/bin/bash

SERVER="./build/epoll_echo"
BENCH_TOOL_PATH="../rust_echo_bench"
BENCH_TOOL="cargo run --release -- --address '127.0.0.1:8080' --number 100 --duration 30 --length 512"
SERVER_STARTUP_DELAY=1
PERF_DURATION=30
OUTPUT_DIR="../results/perf_test_epoll"

FLAMEGRAPH_DIR="/mnt/tools/FlameGraph"

mkdir -p "$OUTPUT_DIR"

cleanup() {
    pkill -f "$SERVER"
    exit 0
}

trap cleanup EXIT

# 启动 server 进程
echo "启动 server..."
$SERVER > "$OUTPUT_DIR/server.log" 2>&1 &
SERVER_PID=$!
sleep $SERVER_STARTUP_DELAY

# 启动基准测试
echo "开始基准测试..."
bash -c "cd '$BENCH_TOOL_PATH' && $BENCH_TOOL" > "$OUTPUT_DIR/bench.log" 2>&1 &
BENCH_PID=$!
sleep 1

# 启动 perf 采样，采集 server 的性能数据
echo "开始 perf 采样..."
perf record -F 99 -p $SERVER_PID -g -o $OUTPUT_DIR/perf.data -- sleep $PERF_DURATION &
PERF_PID=$!

# echo "开始 valgrind..."
# valgrind --tool=massif --massif-out-file=massif.out -p $SERVER_PID &
# ms_print massif.out > $OUTPUT_DIR/memory_report.txt

# 等待 perf 测试完成
echo "等待测试完成..."
sleep $((PERF_DURATION + 5))

# 生成性能报告
echo "生成性能报告..."
perf report -i $OUTPUT_DIR/perf.data --stdio > $OUTPUT_DIR/perf_report.txt

# 如果 FlameGraph 目录存在，则生成火焰图
if [ -d "$FLAMEGRAPH_DIR" ]; then
    echo "生成火焰图..."
    perf script -i $OUTPUT_DIR/perf.data | $FLAMEGRAPH_DIR/stackcollapse-perf.pl > $OUTPUT_DIR/out.perf-folded
    $FLAMEGRAPH_DIR/flamegraph.pl $OUTPUT_DIR/out.perf-folded > $OUTPUT_DIR/flamegraph.svg
fi

echo "测试完成！结果保存在目录: $OUTPUT_DIR"
