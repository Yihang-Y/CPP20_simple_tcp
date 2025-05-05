#!/usr/bin/env python3
import socket
import time
import threading
import argparse
import sys
import subprocess
import os
import signal
from typing import List, Tuple

class EchoServer:
    def __init__(self, server_type: str = "coroutine"):
        self.server_type = server_type
        self.process = None
        self.port = 8080
        
    def start(self):
        """启动Echo服务器"""
        if self.server_type == "coroutine":
            cmd = "cd coroutine_echo/build && ./simple_tcp"
        else:
            cmd = "cd epoll_echo/build && ./epoll_echo"
            
        print(f"启动 {self.server_type} Echo服务器...")
        self.process = subprocess.Popen(cmd, shell=True)
        time.sleep(1)  # 等待服务器启动
        
    def stop(self):
        """停止Echo服务器"""
        if self.process:
            print(f"停止 {self.server_type} Echo服务器...")
            self.process.terminate()
            self.process.wait()

def create_test_message(length: int) -> bytes:
    """创建指定长度的测试消息"""
    return b'X' * length

def echo_client(server_address: Tuple[str, int], message: bytes, num_messages: int) -> Tuple[int, float]:
    """单个客户端测试函数"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(server_address)
        
        start_time = time.time()
        successful_messages = 0
        
        for _ in range(num_messages):
            sock.sendall(message)
            response = sock.recv(len(message))
            if response == message:
                successful_messages += 1
        
        end_time = time.time()
        sock.close()
        return successful_messages, end_time - start_time
    
    except Exception as e:
        print(f"客户端错误: {e}")
        return 0, 0

def run_test(server_address: Tuple[str, int], num_clients: int, message_length: int, messages_per_client: int) -> None:
    """运行并发测试"""
    message = create_test_message(message_length)
    threads = []
    results = []
    
    print(f"开始测试: {num_clients} 个客户端, 每个客户端发送 {messages_per_client} 条消息")
    
    # 创建并启动客户端线程
    for _ in range(num_clients):
        thread = threading.Thread(
            target=lambda: results.append(
                echo_client(server_address, message, messages_per_client)
            )
        )
        threads.append(thread)
        thread.start()
    
    # 等待所有线程完成
    for thread in threads:
        thread.join()
    
    # 计算统计结果
    total_messages = sum(success for success, _ in results)
    total_time = max(time for _, time in results)
    
    if total_time > 0:
        throughput = total_messages / total_time
        print(f"\n测试结果:")
        print(f"总消息数: {total_messages}")
        print(f"总时间: {total_time:.2f} 秒")
        print(f"吞吐量: {throughput:.2f} 消息/秒")
        print(f"平均延迟: {(total_time / total_messages * 1000):.2f} 毫秒/消息")
    else:
        print("测试失败")

def main():
    parser = argparse.ArgumentParser(description='协程Echo服务器测试工具')
    parser.add_argument('--host', default='127.0.0.1', help='服务器主机地址')
    parser.add_argument('--port', type=int, default=8080, help='服务器端口')
    parser.add_argument('--clients', type=int, default=10, help='并发客户端数量')
    parser.add_argument('--message-length', type=int, default=512, help='消息长度（字节）')
    parser.add_argument('--messages', type=int, default=100, help='每个客户端发送的消息数量')
    parser.add_argument('--server-type', choices=['coroutine', 'epoll'], default='coroutine',
                      help='服务器类型 (coroutine 或 epoll)')
    
    args = parser.parse_args()
    
    print("Echo服务器测试工具")
    print("=" * 50)
    print(f"服务器类型: {args.server_type}")
    print(f"服务器地址: {args.host}:{args.port}")
    print(f"并发客户端: {args.clients}")
    print(f"消息长度: {args.message_length} 字节")
    print(f"每个客户端消息数: {args.messages}")
    print("=" * 50)
    
    # 创建并启动服务器
    server = EchoServer(args.server_type)
    
    try:
        server.start()
        run_test(
            (args.host, args.port),
            args.clients,
            args.message_length,
            args.messages
        )
    except KeyboardInterrupt:
        print("\n测试被用户中断")
    except Exception as e:
        print(f"\n测试出错: {e}")
    finally:
        server.stop()

if __name__ == "__main__":
    main() 