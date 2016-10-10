#ifndef __FILE_UTIL_H__
#define __FILE_UTIL_H__

#pragma once


#include <stdexcept>

#include <fcntl.h>


void make_fd_non_blocking(int fd)
{
    int flags;

    flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error{"Failed to get flags of the socket."};
    } else {
        flags |= O_NONBLOCK;
        if (::fcntl(fd, F_SETFL, flags) == -1) {
            throw std::runtime_error{"Failed to set flags on socket."};
        }
    }
}

#endif /* __FILE_UTIL_H__ */
