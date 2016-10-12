#ifndef __UTIL_H__
#define __UTIL_H__

#pragma once


#include <stdexcept>

#include <fcntl.h>


namespace util {

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

} /* namespace util */

#endif /* __UTIL_H__ */
