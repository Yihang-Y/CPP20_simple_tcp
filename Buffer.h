#pragma once
#include <vector>
#include "Task.h"

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

class RecvAwaitable : public Awaitable{
public:
    RecvAwaitable(RecvAttr attr, int* res) : Awaitable{attr.sqe, res}{
        io_uring_prep_readv(attr.sqe, attr.fd, attr.buf, 2, 0);
    }
};

class WriteAwaitable : public Awaitable{
public:
    WriteAwaitable(WriteAttr attr, int* res) : Awaitable{attr.sqe, res}{
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

class Buffer {
public:
// friend class Connection;
    static const size_t kCheapPrepend = 8;
    // 1KB initial size
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer(kCheapPrepend + initialSize),
          readerIndex(kCheapPrepend),
          writerIndex(kCheapPrepend) {}

    size_t readableBytes() const {
        return writerIndex - readerIndex;
    }

    size_t writableBytes() const {
        return buffer.size() - writerIndex;
    }

    size_t prependableBytes() const {
        return readerIndex;
    }

    size_t size() const {
        return buffer.size();
    }

    char* begin() {
        return &*buffer.begin();
    }

    const char* peek() {
        return begin() + readerIndex;
    }

    void makeSpace(size_t len) {
        // if there is not enough space, resize the buffer
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            // because of the resize, so this class will not use iteator
            // buffer.resize(writerIndex + len);
            std::vector<char> newBuffer(kCheapPrepend + readableBytes() + len);
            std::copy(begin() + readerIndex,
                      begin() + writerIndex,
                      newBuffer.begin() + kCheapPrepend);

            buffer.swap(newBuffer);

            readerIndex = kCheapPrepend;
            writerIndex = readerIndex + readableBytes();
        }
        // some bytes have been read, move the remaining data to the front
        else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex,
                      begin() + writerIndex,
                      begin() + kCheapPrepend);
            readerIndex = kCheapPrepend;
            writerIndex = readerIndex + readable;
        }
    }

    std::string retrieveAsString(size_t len) {
        std::string str(begin() + readerIndex, len);
        readerIndex += len;
        return str;
    }

    void append(const char* data, size_t len) {
        // ensureWritableBytes(len);
        if(writableBytes() < len){
            makeSpace(len);
        }
        std::copy(data, data + len, begin() + writerIndex);
        writerIndex += len;
    }

    Task<int> recv(io_uring *ring, int fd) {
        char extraBuf[65536];

        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        struct iovec vec[2];
        vec[0].iov_base = begin() + writerIndex;
        vec[0].iov_len = writableBytes();
        vec[1].iov_base = extraBuf;
        vec[1].iov_len = sizeof(extraBuf);

        int res = co_await RecvAttr{{sqe}, fd, vec, 2};
        if (res == 0) {
            std::cout << "Connection closed" << std::endl;
            co_return 0;
        }
        else if (res < 0) {
            std::cout << "ERROR: "<< strerror(-res) << std::endl;
            co_return -1;
        }
        else if (res <= writableBytes()) {
            writerIndex += res;
        }
        else {
            size_t writable = writableBytes();
            writerIndex = size();
            append(extraBuf, res - writable);
        }
        co_return res;
    }

    Task<int> send(io_uring *ring, int fd, size_t len) {
        while (len > 0){
            if (readableBytes() < len){
                std::cout << "ERROR: Not enough data to send" << std::endl;
                co_return -1;
            }
            io_uring_sqe *sqe = io_uring_get_sqe(ring);
            char* buf = begin() + readerIndex;

            int res = co_await WriteAttr{{sqe}, fd, buf, len};
            if (res < 0) {
                std::cout << "ERROR: "<< strerror(-res) << std::endl;
                co_return -1;
            }
            readerIndex += res;
            len -= res;
        }
        co_return len;
    }

private:
    std::vector<char> buffer;
    size_t readerIndex;
    size_t writerIndex;

    static const char kCRLF[];
};

const char Buffer::kCRLF[] = "\r\n";