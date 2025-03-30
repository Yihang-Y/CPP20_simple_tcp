#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

class Buffer {
public:
    static const size_t kInitialSize = 1024;
    
    Buffer() : buffer_(kInitialSize), readerIndex_(0), writerIndex_(0) {}
    
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    
    const char* peek() const { return begin() + readerIndex_; }
    
    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }
    
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            readerIndex_ += len;
        } else {
            retrieveAll();
        }
    }
    
    void retrieveAll() {
        readerIndex_ = 0;
        writerIndex_ = 0;
    }
    
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    
    ssize_t readFromFd(int fd, int* savedErrno) {
        char extrabuf[65536];
        struct iovec vec[2];
        const size_t writable = writableBytes();
        
        vec[0].iov_base = beginWrite();
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof extrabuf;
        
        const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
        const ssize_t n = ::readv(fd, vec, iovcnt);
        
        if (n < 0) {
            *savedErrno = errno;
        } else if (static_cast<size_t>(n) <= writable) {
            writerIndex_ += n;
        } else {
            writerIndex_ = buffer_.size();
            append(extrabuf, n - writable);
        }
        
        return n;
    }
    
private:
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }
    
    char* beginWrite() { return begin() + writerIndex_; }
    
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }
    
    void makeSpace(size_t len) {
        if (writableBytes() + readerIndex_ < len) {
            buffer_.resize(writerIndex_ + len);
        } else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin());
            readerIndex_ = 0;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
}; 