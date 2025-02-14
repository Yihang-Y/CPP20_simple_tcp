#pragma once
#include <vector>

class Buffer {
public:
friend class Connection;
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

private:
    std::vector<char> buffer;
    size_t readerIndex;
    size_t writerIndex;

    static const char kCRLF[];
};

const char Buffer::kCRLF[] = "\r\n";