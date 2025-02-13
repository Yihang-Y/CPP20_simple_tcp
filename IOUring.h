#include <liburing.h>
#include <system_error>


class IOUring{


public:
    IOUring(){
        init();
    }
    ~IOUring(){
        io_uring_queue_exit(&ring);
    }
    void init(){
        if (io_uring_queue_init(32, &ring, 0) < 0) {
            throw std::system_error(errno, std::system_category(), "io_uring_queue_init");
        }
    }

    io_uring* getRing(){
        return &ring;
    }

private:
    /* data */
    io_uring ring;
};