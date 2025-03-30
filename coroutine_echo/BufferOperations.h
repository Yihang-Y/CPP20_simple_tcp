#pragma once
#include "Buffer.h"
#include "Task.h"
#include "Awaitable.h"
#include "IoUringSchedulerAdapter.h"

struct RecvAttr : Attr{
    int fd;
    struct iovec* buf;
    size_t size;
};

struct WriteAttr : Attr{
    int fd;
    const char* buf;
    size_t size;
};

class RecvAwaitable : public SubmitAwaitable{
public:
    RecvAwaitable(RecvAttr attr, int* res) : SubmitAwaitable{attr.sqe, res}{
        io_uring_prep_readv(attr.sqe, attr.fd, attr.buf, 2, 0);
    }
};

class WriteAwaitable : public SubmitAwaitable{
public:
    WriteAwaitable(WriteAttr attr, int* res) : SubmitAwaitable{attr.sqe, res}{
        io_uring_prep_send(attr.sqe, attr.fd, attr.buf, attr.size, 0);
    }
};

template<>
struct awaitable_traits<RecvAttr>{
    using type = RecvAwaitable;
};

template<>
struct awaitable_traits<WriteAttr>{
    using type = WriteAwaitable;
};


Task<int> recv(Buffer& buffer, int fd) {
    char extraBuf[65536];

    io_uring_sqe *sqe = io_uring_get_sqe(getScheduler().getRing());
    struct iovec vec[2];
    vec[0].iov_base = buffer.begin() + buffer.writerIndex;
    vec[0].iov_len = buffer.writableBytes();
    vec[1].iov_base = extraBuf;
    vec[1].iov_len = sizeof(extraBuf);

    int res = co_await RecvAttr{{sqe}, fd, vec, 2};
    if (res == 0) {
        co_return 0;
    } else if (res < 0) {
        std::cout << "ERROR: " << strerror(-res) << std::endl;
        co_return -1;
    } else if (res <= buffer.writableBytes()) {
        buffer.writerIndex += res;
    } else {
        size_t writable = buffer.writableBytes();
        buffer.writerIndex = buffer.size();
        buffer.append(extraBuf, res - writable);
    }
    co_return res;
}

Task<int> send(Buffer& buffer, int fd, size_t len) {
    while (len > 0) {
        if (buffer.readableBytes() < len) {
            std::cout << "ERROR: Not enough data to send" << std::endl;
            co_return -1;
        }
        io_uring_sqe *sqe = io_uring_get_sqe(getScheduler().getRing());
        char* buf = buffer.begin() + buffer.readerIndex;

        int res = co_await WriteAttr{{sqe}, fd, buf, len};
        if (res < 0) {
            std::cout << "ERROR: " << strerror(-res) << std::endl;
            co_return -1;
        }
        buffer.readerIndex += res;
        len -= res;
    }
    co_return len;
} 