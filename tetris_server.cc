#include "connection.h"
#include "csv.h"
#include "mapping.h"
#include "path_util.h"
#include "socket.h"
#include "tetris.h"

#include <algorithm>
#include <iostream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <errno.h>
#include <sched.h>
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

   public:
    ConnectionPtr           connection;
    std::string             exec;
    int                     pid;
    bool                    dynamic_client;
    std::vector<Thread>     threads;
    std::vector<Mapping>    mappings;
    Mapping                 active_mapping;

   public:
    Client(const ConnectionPtr& conn) :
        connection{conn}, exec{}, pid{-1}, dynamic_client{false}, threads{}, mappings{}, active_mapping{}
    {
        std::cout << "New client created." << std::endl;
    }

    ~Client()
    {
        std::cout << "Client removed." << std::endl;
    }

    cpu_set_t* new_thread(const std::string& name, int pid)
    {
        auto i = std::find_if(threads.begin(), threads.end(), [&](const Thread& t) { 
                return t.name == name;
        });

        if (i != threads.end()) {
            threads.emplace_back(name, pid);
        }

        cpu_set_t* mask = CPU_ALLOC(8);
        CPU_ZERO(mask);
	if (dynamic_client) {
	    for (std::pair<std::string,int> p : active_mapping.thread_map) {
                CPU_SET(p.second,mask);
		std::cout << "Enabling cpu " << p.second << "(for " << p.first << ")" << std::endl;
            }
        } else {
            CPU_SET(active_mapping.thread_map.at(name),mask);
        }
	return mask;
    }
};

class Manager
{
   private:
    std::map<int, Client>   _clients;
    std::string             _mappings_path;
    std::map<std::string, std::vector<Mapping>> _mappings;

    std::vector<Mapping> parse_mapping(const std::string& file)
    {
        CSVData data{file};
        std::vector<Mapping> result;

        for (const auto& row : data.row_iter()) {
            std::vector<std::pair<std::string, std::string>> thread_map;

            for (const auto& col : data.columns()) {
                if (string_util::starts_with(col, "t_")) {
                    std::string thread_name = col.substr(2);
                    std::string cpu_name = row(col);

                    thread_map.emplace_back(thread_name, cpu_name);
                }
            }

            auto name = row.fixed();
            auto exec_time = std::stod(row("executionTime"));
            auto energy = std::stod(row("energyConsumption"));
            auto memory = std::stod(row("memorySize"));

            result.emplace_back(name, thread_map, exec_time, energy, memory);
        }

        return result;
    }

    template <typename Criteria = std::function<double(const Mapping&)>, typename Comp = std::function<bool(double, double)>>
    Mapping select_best_mapping(Client& c,
            Criteria criteria = [] (const Mapping& m) { return m.exec_time; },
            Comp better = [] (double a, double b) { return a < b; })
    {
        auto best = c.mappings.front();

        for (const auto& m : c.mappings) {
            if (better(criteria(m), criteria(best)))
                best = m;
        }

        return best;
    }

    Mapping use_preferred_mapping(Client& c, const std::string& preferred_mapping_name)
    {
        for (const auto& m : c.mappings) {
            if (m.name == preferred_mapping_name)
                return m;
        }

        /* We could not find the specified mapping. Use the best one. */
        return select_best_mapping(c);
    }

   public:
    explicit Manager(const std::string& mappings_path) :
        _clients{}, _mappings_path{mappings_path}, _mappings{}
    {
        update_mappings();
    }

    void client_connect(int fd, const ConnectionPtr& conn)
    {
        _clients.emplace(fd, conn);
    }

    void client_disconnect(int fd)
    {
        _clients.erase(fd);
    }

    bool client_message(int fd)
    try {
        Client& c = _clients.at(fd);
        ConnectionPtr conn = c.connection;

        bool done = false;
        bool close = false;

        while (!done) {
            TetrisData message;

            auto res = conn->read(message);

            if (res == Connection::InState::DONE) {
                /* We are done processing. So return. */
                done = true;
            } else if (res == Connection::InState::CLOSED) {
                /* We are done processing and the remote site closed the
                 * connection. */
                close = true;
                done = true;
            } else {
                /* There is some data to process. Handle it. */
                switch (message.op) {
                    case TetrisData::NEW_CLIENT: {
                        int pid = message.new_client_data.pid;
                        std::string exec = string_util::strip(path_util::basename(message.new_client_data.exec));
                        bool managed;
                        try {
                            /* Update the client data. */
                            c.pid = pid;
                            c.exec = exec;
                            c.dynamic_client = message.new_client_data.dynamic_client;
                            c.mappings = _mappings.at(exec);

                            if (message.new_client_data.has_preferred_mapping) {
                                std::string preferred_mapping = string_util::strip(message.new_client_data.preferred_mapping);
                                c.active_mapping = use_preferred_mapping(c, preferred_mapping);
                            } else {
                                c.active_mapping = select_best_mapping(c);
                            }

                            std::cout << "New client registered: '" << exec << "' [" << pid << "]" << std::endl;
                            std::cout << "Run client according to mapping " << c.active_mapping.name << "." << std::endl;
                            std::cout << "Client is " << ((c.dynamic_client)?"managed dynamically by CFS.":"mapped statically.") << std::endl;

                            /* We will manage this client. */
                            managed = true;
                        } catch (std::out_of_range&) {
                            std::cout << "Unknown client: '" << exec << "' [" << pid << "]" << std::endl;
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        TetrisData ack;
                        ack.op = TetrisData::NEW_CLIENT_ACK;
                        ack.new_client_ack_data.managed = managed;

                        if (conn->write(ack) != Connection::OutState::DONE) {
                            std::cerr << "Failed to acknowledge the new-client message." << std::endl;
                        }

                        /* If we don't manage this client we can close its connection. */
                        close = !managed;
                        break;
                    }
                    case TetrisData::Operations::NEW_THREAD: {
                        int tid = message.new_thread_data.tid;
                        std::string name = string_util::strip(message.new_thread_data.name);
                        bool managed;
                        try {
                            /* Update the client data. */
                            cpu_set_t* mask = c.new_thread(name, tid);

                            std::cout << "New thread '" << name << "' [" << tid << "] for client '" << c.exec << "'"
                                << " registered." << std::endl;

                            /* Set the affinity for the thread. */
                            if (sched_setaffinity(tid, CPU_ALLOC_SIZE(8), mask) != 0) {
                                std::cerr << "Failed to set cpu affinity for the thread." << std::endl
                                    << strerror(errno) << std::endl;
                            }
                            std::cerr << "Set affinity" << std::endl;
			    CPU_FREE(mask);
                            std::cerr << "Freed mask" << std::endl;
                            managed = true;
                        } catch (std::out_of_range) {
                            std::cout << "Unknown thread '" << name << "' for client '" << c.exec << "'" << std::endl;
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        TetrisData ack;
                        ack.op = TetrisData::NEW_THREAD_ACK;
                        ack.new_thread_ack_data.managed = managed;
                        ack.new_thread_ack_data.cpu = -1; //cpu;

                        if (conn->write(ack) != Connection::OutState::DONE) {
                            std::cerr << "Failed to acknowledge the new-thread message." << std::endl;
                        }

                        break;
                    }
                    default:
                        std::cout << "Other message received." << std::endl;
                }
            }
        }

        return close;
    } catch (std::out_of_range) {
        std::cerr << "Unknown client: " << fd << "." << std::endl;
        return true;
    }

    void update_mappings()
    {
        std::cout << "Update mapping database (" << _mappings_path << ")." << std::endl;
        _mappings.clear();

        path_util::for_each_file(_mappings_path, [&](const std::string& file) -> void {
            if (path_util::extension(file) == ".csv") {
                std::string program = string_util::strip(path_util::filename(file));
                std::cout << " -> found mapping for '" << program << "'" << std::endl;

                _mappings.emplace(program, parse_mapping(file));
            }
        });
    }
};


void usage()
{
    std::cout << "usage: tetrisserver [-h] [MAPPINGS]" << std::endl
        << std::endl
        << "Options:" << std::endl
        << "   -h, --help           show this help message." << std::endl
        << std::endl
        << "Positionals:" << std::endl
        << " MAPPINGS               path the folder with the per-app mappings." << std::endl;
}

int main(int argc, char *argv[])
{
    /* Parsing command line arguments. */
    std::string mappings_path;

    if (argc > 2) {
        usage();
        return 1;
    } else if (argc == 1) {
        mappings_path = path_util::getcwd();
    } else {
        std::string arg{argv[1]};
        if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else {
            mappings_path = path_util::abspath(path_util::expanduser(arg));
        }
    }

    /* Setting up the manager. */
    Manager manager{mappings_path};

    /* Setting up the server socket */
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
        sigaddset(&sigmask, SIGUSR1);

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

                    if (siginfo.ssi_signo == SIGUSR1) {
                        std::cout << "Refreshing mapping database." << std::endl;
                        manager.update_mappings();
                    } else {
                        done = 1;
                    }
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
