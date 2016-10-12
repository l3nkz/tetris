#ifndef __SOCKET_H__
#define __SOCKET_H__

#pragma once

#include "lock_util.h"
#include "path_util.h"
#include "util.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


class Socket : public Lockable<Socket>
{
   private:
    int             _fd;
    sockaddr_un     _sock;
    bool            _blocking;

    void close() {
        if (_fd == -1)
            return;

        ::unlink(_sock.sun_path);
        ::close(_fd);
        _fd = -1;
    }

   public:
    Socket() :
        _fd{-1}, _sock{}, _blocking{true}
    {}

    explicit Socket(const std::string& sock_path) :
        _fd{-1}, _sock{}, _blocking{true}
    {
        open(sock_path);
    }

    Socket(int fd, const sockaddr_un& sock, bool blocking=true) :
        _fd{fd}, _sock{sock}, _blocking{blocking}
    {}

    Socket(const Socket&) = delete;
    Socket(Socket&& o) :
        _fd{o._fd}, _sock{o._sock}
    {
        o._fd = -1;
    }

    ~Socket()
    {
        close();
    }

    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&& o)
    {
        close();

        _fd = o._fd;
        _sock = o._sock;
        _blocking = o._blocking;

        o._fd = -1;

        return *this;
    }

    int fd() const
    {
        if (_fd == -1) {
            throw std::runtime_error{"Socket not initialized."};
        }

        return _fd;
    }

    const char* path() const
    {
        if (_fd == -1) {
            throw std::runtime_error{"Socket not initialized."};
        }

        return _sock.sun_path;
    }

    void open(const std::string& sock_path)
    {
        if (_fd != -1) {
            throw std::runtime_error{"Socket already initialized."};
        }

        /* First check if the path already exists. */
        if (path_util::exists(sock_path)) {
            throw std::runtime_error{"The socket is already taken."};
        }

        /* Create the socket */
        if ((_fd = ::socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw std::runtime_error{"Failed to acquire socket fd."};
        }

        _sock.sun_family = AF_UNIX;
        std::strcpy(_sock.sun_path, sock_path.c_str());
        if (::bind(_fd, reinterpret_cast<sockaddr*>(&_sock), sizeof(_sock)) == -1) {
            ::close(_fd);
            _fd = -1;
            throw std::runtime_error{"Failed to bind to socket."};
        }
    }

    void non_blocking()
    {
        if (_fd == -1) {
            throw std::runtime_error{"Socket not initialized."};
        }

        if (_blocking) {
            util::make_fd_non_blocking(_fd);
            _blocking = false;
        }
    }

    void listening()
    {
        if (_fd == -1) {
            throw std::runtime_error{"Socket not initialized."};
        }

        /* Listen on the socket */
        if (::listen(_fd, SOMAXCONN) == -1) {
            ::close(_fd);
            throw std::runtime_error{"Failed to listen on socket."};
        }
    }
};

using LockedSocket = Locked<Socket>;

#endif /* __SOCKET_H__ */
