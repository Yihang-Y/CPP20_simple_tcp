#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import json
from typing import List, Dict
import matplotlib.pyplot as plt
import numpy as np
import re

OUTPUT_DIR = "results/benchmark"

class EchoServer:
    def __init__(self, name: str, run_cmd: List[str]):
        self.name = name
        self.run_cmd = run_cmd
        self.process = None

    def start(self):
        print(f"Starting {self.name}...")
        self.process = subprocess.Popen(self.run_cmd, shell=True)
        time.sleep(1)  # 等待服务器启动

    def stop(self):
        if self.process:
            self.process.terminate()
            self.process.wait()

def parse_benchmark_output(output: str) -> Dict:
    # 解析Rust基准测试工具的输出
    speed_match = re.search(r'Speed: (\d+) request/sec', output)
    requests_match = re.search(r'Requests: (\d+)', output)
    responses_match = re.search(r'Responses: (\d+)', output)
    
    if not all([speed_match, requests_match, responses_match]):
        raise ValueError("无法解析基准测试输出")
    
    return {
        "requests_per_second": int(speed_match.group(1)),
        "total_requests": int(requests_match.group(1)),
        "total_responses": int(responses_match.group(1))
    }

def run_benchmark(num_clients: int, duration: int = 30, message_length: int = 512) -> Dict:
    # 使用Rust基准测试工具
    bench_cmd = [
        "cd", "rust_echo_bench",
        "&&", "cargo", "run", "--release", "--",
        "--address", "127.0.0.1:8080",
        "--number", str(num_clients),
        "--duration", str(duration),
        "--length", str(message_length)
    ]
    
    try:
        print(" ".join(bench_cmd))
        result = subprocess.run(" ".join(bench_cmd), shell=True, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"基准测试失败: {result.stderr}")
        
        return parse_benchmark_output(result.stdout)
    except Exception as e:
        print(f"运行基准测试时出错: {e}")
        return None

def plot_results(results: Dict[str, List[Dict]]):
    plt.figure(figsize=(10, 6))
    
    for server_name, data in results.items():
        clients = [r["clients"] for r in data]
        throughput = [r["requests_per_second"] for r in data]
        plt.plot(clients, throughput, marker='o', label=server_name)
    
    plt.xlabel("Number of Clients")
    plt.ylabel("Requests per Second")
    plt.title("Echo Server Performance Comparison")
    plt.legend()
    plt.grid(True)
    plt.savefig(os.path.join(OUTPUT_DIR, "benchmark_results.png"))
    plt.close()

def main():
    # 定义服务器配置
    servers = {
        "coroutine_echo": EchoServer(
            "Coroutine Echo",
            ["cd coroutine_echo/build && ./simple_tcp"]
        ),
        "epoll_echo": EchoServer(
            "Epoll Echo",
            ["cd epoll_echo/build && ./epoll_echo"]
        )
    }

    # 测试参数
    # client_counts = [10, 50, 100, 200, 500]
    client_counts = [1000, 2000, 5000]
    results = {}

    for server_name, server in servers.items():
        print(f"\nTesting {server_name}...")
        server_results = []
        
        for num_clients in client_counts:
            server.start()
            try:
                result = run_benchmark(num_clients)
                if result:
                    result["clients"] = num_clients
                    server_results.append(result)
                    print(f"Clients: {num_clients}, Throughput: {result['requests_per_second']} req/s")
            finally:
                server.stop()
        
        results[server_name] = server_results

    # 保存结果到JSON文件
    with open(os.path.join(OUTPUT_DIR, "benchmark_results.json"), "w") as f:
        json.dump(results, f, indent=2)

    # 绘制图表
    plot_results(results)
    print("\nBenchmark results have been saved to benchmark_results.json and benchmark_results.png")

if __name__ == "__main__":
    main() 