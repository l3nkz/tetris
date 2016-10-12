#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#pragma once


#include "util.h"
#include "path_util.h"
#include "tetris.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


class Connection
{
   public:
    enum class InState {
        MORE = 1,
        DONE = 2,
        CLOSED = 3
    };

    enum class OutState {
        DONE = 1,
        RETRY = 2,
    };

   private:
    int             _fd;
    sockaddr_un     _sock;
    bool            _blocking;

    void close()
    {
        if (_fd == -1)
            return;

        ::close(_fd);
        _fd = -1;
    }

   public:
    Connection() :
        _fd{-1}, _sock{}, _blocking{true}
    {}

    explicit Connection(const std::string& sock_path) :
        _fd{-1}, _sock{}, _blocking{true}
    {
        connect(sock_path);
    }

    Connection(int fd, const sockaddr_un& sock, bool blocking=true) :
        _fd{fd}, _sock{sock}, _blocking{blocking}
    {}

    Connection(const Connection&) = delete;
    Connection(Connection&& o) :
        _fd{o._fd}, _sock{o._sock}, _blocking{o._blocking}
    {
        o._fd = -1;
    }

    ~Connection()
    {
        close();
    }

    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&& o)
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
            throw std::runtime_error{"Connection not initialized."};
        }

        return _fd;
    }

    const char* path() const
    {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        return _sock.sun_path;
    }

    void connect(const std::string& sock_path)
    {
        if (_fd != -1) {
            throw std::runtime_error{"The connection is already initialized."};
        }

        /* Check if the socket already exists. */
        if (!path_util::exists(sock_path)) {
            throw std::runtime_error{"The specified socket file does not exist."};
        }

        /* Create the socket. */
        if ((_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw std::runtime_error{"Failed to acquire socket fd."};
        }

        _sock.sun_family = AF_UNIX;
        std::strcpy(_sock.sun_path, sock_path.c_str());
        if (::connect(_fd, reinterpret_cast<sockaddr*>(&_sock), sizeof(_sock)) == -1) {
            ::close(_fd);
            _fd = -1;
            throw std::runtime_error{"Failed to connect to socket."};
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

    InState read(TetrisData& data) {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        ssize_t size = ::read(_fd, &data, sizeof(data));
        if (size == -1) {
            if (errno == EAGAIN && !_blocking)
                return InState::DONE;

            throw std::runtime_error{"Read failed."};
        } else if (size == 0) {
            return InState::CLOSED;
        } else if (size != sizeof(data)) {
            throw std::runtime_error{"Failed to read complete data!"};
        }

        return _blocking ? InState::DONE : InState::MORE;
    }

    OutState write(const TetrisData& data) {
        if (_fd == -1) {
            throw std::runtime_error{"Connection not initialized."};
        }

        ssize_t size = ::write(_fd, &data, sizeof(data));
        if (size == -1) {
            if (errno == EAGAIN && !_blocking)
                return OutState::RETRY;

            throw std::runtime_error{"Write failed."};
        } else if (size != sizeof(data)) {
            throw std::runtime_error{"Failed to write complete data!"};
        }

        return OutState::DONE;
    }
};

#endif /* __CONNECTION_H__ */
