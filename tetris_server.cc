#include "connection.h"
#include "socket.h"
#include "tetris.h"

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <errno.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>


const static int MAXEVENTS = 100;


using ConnectionPtr = std::shared_ptr<Connection>;

class Client
{
   public:
    struct Thread
    {
        std::string     name;
        int             pid;

        Thread(const std::string& name, int pid) :
            name{name}, pid{pid}
        {}
    };

    struct Mapping
    {
        std::map<std::string, int>  mapping;
        double                      performance;
        double                      energy;
        double                      memory;
    };

   private:
    ConnectionPtr       _connection;
    std::string         _exec;
    int                 _pid;
    std::vector<Thread> _threads;
    std::vector<Mapping> _mappings;

   public:
    Client(const ConnectionPtr& conn) :
        _connection{conn}, _exec{}, _pid{-1}, _threads{}, _mappings{}
    {
        std::cout << "New client created." << std::endl;
    }

    ~Client()
    {
        std::cout << "Client removed." << std::endl;
    }

    bool message()
    {
        while (1) {
            TetrisData data;

            auto res = _connection->read(data);

            if (res == Connection::InState::DONE) {
                /* We are done processing. So return. */
                return false;
            } else if (res == Connection::InState::CLOSED) {
                /* We are done processing and the remote site closed the
                 * connection. */
                return true;
            } else {
                /* There is some data to process. Handle it. */
                switch (data.op) {
                    case TetrisData::NEW_CLIENT:
                        std::cout << "New client registered." << std::endl
                            << " > Exec: " << data.new_client_data.exec << std::endl
                            << " > Pid: " << data.new_client_data.pid << std::endl;
                        break;
                    default:
                        std::cout << "Other message received." << std::endl;
                }
            }
        }
    }
};

class Manager
{
   private:
    std::map<int, Client> _clients;

   public:
    void client_connect(int fd, const ConnectionPtr& conn)
    {
        _clients.emplace(fd, conn);
    }

    void client_disconnect(int fd)
    {
        _clients.erase(fd);
    }

    bool client_message(int fd)
    {
        try {
            return _clients.at(fd).message();
        } catch (std::out_of_range) {
            std::cerr << "Unknown client: " << fd << "." << std::endl;
            return true;
        }
    }
};

int main(int argc, char *argv[])
{
    Manager manager;
    Socket server_sock;
    int sock_fd = -1;
    try {
        server_sock.open(SERVER_SOCKET);
        server_sock.non_blocking();
        server_sock.listening();
        sock_fd = server_sock.fd();
    } catch(std::runtime_error& e) {
        std::cerr << "Failed to open socket" << std::endl
            << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server socket initialized at " << server_sock.path() << " ("
        << sock_fd << ")." << std::endl;

    /* Setup signal handling */
    int sig_fd = -1;
    {
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGABRT);
        sigaddset(&sigmask, SIGHUP);
        sigaddset(&sigmask, SIGINT);
        sigaddset(&sigmask, SIGQUIT);
        sigaddset(&sigmask, SIGTERM);

        /* First block the signals. */
        sigprocmask(SIG_BLOCK, &sigmask, nullptr);

        /* And create a signal fd where these signals are managed. */
        sig_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
        if (sig_fd == -1) {
            std::cerr << "Failed to create signal fd." << std::endl
                << strerror(errno) << std::endl;
            return 1;
        }
    }

    /* Setup the epoll event loop. */
    int epoll_fd = -1;
    {
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            std::cerr << "Failed to initialize epoll." << std::endl
                << strerror(errno) << std::endl;
            return 1;
        }

        epoll_event e;
        e.data.fd = sock_fd;
        e.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &e) == -1) {
            std::cerr << "Failed to add server socket to epoll." << std::endl
                << strerror(errno) << std::endl;
            return 1;
        }

        e.data.fd = sig_fd;
        e.events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sig_fd, &e) == -1) {
            std::cerr << "Failed to add signal fd to epoll." << std::endl
                << strerror(errno) << std::endl;
            return 1;
        }
    }

    /* The event loop */
    epoll_event events[MAXEVENTS];
    bool done = false;

    while (!done) {
        int n;

        n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

        for (int i = 0; i < n; ++i) {
            epoll_event *cur = &events[i];

            if (cur->data.fd == sock_fd) {
                /* There are a new connections at the socket.
                 * Connect with all of them.
                 */
                while (1) {
                    sockaddr_un in_sock;
                    socklen_t in_sock_size = sizeof(in_sock);
                    int infd = ::accept(sock_fd, reinterpret_cast<sockaddr*>(&in_sock), &in_sock_size);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We connected to all possible connections already.
                             * Continue with the main loop.
                             */
                            break;
                        } else {
                            std::cerr << "An error happened while accepting a connection." << std::endl
                                << strerror(errno) << std::endl;
                            break;
                        }
                    }

                    /* Make the new socket non-blocking and add it to epoll. */
                    {
                        ConnectionPtr in_conn = std::make_shared<Connection>(infd, in_sock);
                        in_conn->non_blocking();
                        epoll_event e;
                        e.data.fd = infd;
                        e.events = EPOLLIN;

                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &e) == -1) {
                            std::cerr << "Failed to add new connection to epoll." << std::endl
                                << strerror(errno) << std::endl;
                            ::close(infd);
                        } else {
                            std::cout << "A new client connected (" << infd << ")." << std::endl;

                            manager.client_connect(infd, in_conn);
                        }
                    }
                }
            } else if (cur->data.fd == sig_fd) {
                /* There was a signal delivered to this process. */
                while (1) {
                    signalfd_siginfo siginfo;
                    ssize_t count;

                    count = read(sig_fd, &siginfo, sizeof(siginfo));
                    if (count == -1) {
                        if (errno != EAGAIN) {
                            std::cerr << "An error happened while reading data from signal fd." << std::endl
                                << strerror(errno) << std::endl;
                        }
                        break;
                    }

                    std::cout << "Received a signal (" << siginfo.ssi_signo << ")." << std::endl;
                    done = 1;
                }
            } else if (cur->events & EPOLLIN) {
                /* Some client tried to send us data. */
                std::cout << "The client sent a message." << std::endl;

                if (manager.client_message(cur->data.fd)) {
                    std::cout << "The client disconnected" << std::endl;
                    manager.client_disconnect(cur->data.fd);
                }
            } else if (cur->events & EPOLLHUP) {
                /* Some client disconnected. */
                std::cout << "The client disconnected" << std::endl;
                manager.client_disconnect(cur->data.fd);
            } else {
                std::cerr << "Strange event at fd " << cur->data.fd << std::endl;
                ::close(cur->data.fd);
            }
        }
    }

    std::cout << "Exiting" << std::endl;
    ::close(sig_fd);

    return 0;
}
