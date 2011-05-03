
#ifndef __ARCH_LINUX_LINUX_IO_CALLS_EVENTFD_HPP__
#define __ARCH_LINUX_LINUX_IO_CALLS_EVENTFD_HPP__

#include "arch/linux/disk/linux_io_calls_base.hpp"

class linux_io_calls_eventfd_t : public linux_io_calls_base_t {
public:
    explicit linux_io_calls_eventfd_t(linux_event_queue_t *queue);
    ~linux_io_calls_eventfd_t();

    fd_t aio_notify_fd;
    
public:
    void on_event(int events);
};

#endif // __ARCH_LINUX_LINUX_IO_CALLS_EVENTFD_HPP__
