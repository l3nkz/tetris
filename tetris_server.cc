#include "algorithm.h"
#include "connection.h"
#include "csv.h"
#include "filter.h"
#include "mapping.h"
#include "path_util.h"
#include "socket.h"
#include "string_util.h"
#include "tetris.h"

#include <algorithm>
#include <deque>
#include <iostream>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>


const static int MAXEVENTS = 100;

class NoMappingError : public std::runtime_error
{
   public:
    using std::runtime_error::runtime_error;
};

using ConnectionPtr = std::shared_ptr<Connection>;

class Client
{
   public:
    struct Thread
    {
        std::string     name;
        int             tid;
        cpu_set_t       affinity;

        Thread(const std::string& name, int tid, cpu_set_t affinity) :
            name{name}, tid{tid}, affinity{affinity}
        {}
    };

    class Comp
    {
        std::string     _criteria;
        std::function<bool(const double, const double)> _comp;

       public:
        Comp(const std::string compare_criteria, bool more_is_better) :
            _criteria{compare_criteria}
        {
            if (more_is_better)
                _comp = std::greater<double>();
            else
                _comp = std::less<double>();
        }

        Comp() :
            _criteria{}, _comp{std::less<double>()}
        {}

        bool operator()(const Mapping& other, const Mapping& best)
        {
            return _comp(other.characteristic(_criteria), best.characteristic(_criteria));
        }

        std::string criteria() const
        {
            return _criteria;
        }
    };

   public:
    ConnectionPtr           connection;
    std::string             exec;
    int                     pid;
    bool                    dynamic_client;
    std::vector<Thread>     threads;
    std::vector<Mapping>    mappings;
    Mapping                 active_mapping;

    Filter                  filter;
    Comp                    comp;

   public:
    Client(const Client&) = delete;

    Client(const ConnectionPtr& conn) :
        connection{conn}, exec{}, pid{-1}, dynamic_client{false}, threads{}, mappings{}, active_mapping{},
        filter{}, comp{}
    {
        std::cout << "New client created." << std::endl;
    }

    ~Client()
    {
        std::cout << "Client removed." << std::endl;
    }

    cpu_set_t new_thread(const std::string& name, int pid)
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);

        if (dynamic_client) {
            std::cout << "Enabling all mapping cpus for this client." << std::endl;

            for (auto [name, cpu] : active_mapping.thread_map) {
                CPU_SET(cpu, &mask);

                std::cout << "Enabling cpu " << cpu << "(for thread " << name << ")" << std::endl;
            }
        } else {
            mask = active_mapping.cpu_mask(name);
        }

        auto i = std::find_if(threads.begin(), threads.end(), [&](const Thread& t) {
                return t.name == name;
        });

        if (i == threads.end()) {
            threads.emplace_back(name, pid, mask);
        } else {
            std::cerr << "WARNING! Duplicate thread ..." << std::endl;
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
            std::vector<std::pair<std::string, std::string>> threads;
            std::vector<std::pair<std::string, std::string>> characteristics;

            for (const auto& col : row.names()) {
                if (string_util::starts_with(col, "t_")) {
                    std::string thread_name = col.substr(2);
                    std::string cpu_name = row(col);

                    threads.emplace_back(thread_name, cpu_name);
                } else {
                    /* All the other columns are characteristics of the mapping */
                    std::string value = row(col);

                    characteristics.emplace_back(col, value);
                }
            }

            auto name = row.fixed();

            result.emplace_back(name, threads, characteristics);
        }

        return result;
    }

    Mapping select_best_mapping(Client& c)
    {
        CPUList occupied_cpus{};

        for (const auto& [name, cl] : _clients) {
            occupied_cpus |= cl.active_mapping.cpus;
        }

        /* Get all the tetris mappings for this client */
        auto possible_mappings = tetris_mappings(c.mappings, occupied_cpus);
        if (possible_mappings.empty())
            throw NoMappingError("Can't find a proper mapping for the client");
        else
            std::cout << "There are " << possible_mappings.size() << " mappings for this client." << std::endl;

        /* Now select the best one of the available ones according to the given
         * criteria and the given comperator */
        auto comp = [&c] (const Mapping& first, const Mapping& second) -> bool {
            return c.comp(first, second);
        };
        auto filter = [&c] (const Mapping& m) -> bool {
            return c.filter(m);
        };

        auto best = possible_mappings.front();
        for (const auto& m : possible_mappings) {
            if (comp(m, best) && filter(m)) {
                std::cout << "Found better mapping: " << m.name << " (" << m.characteristic(c.comp.criteria())
                    << ") is better than " << best.name << " (" << best.characteristic(c.comp.criteria()) << ")"
                    << std::endl;
                best = m;
            }
        }

        std::cout << "The best mapping is: " << best.name << " (" << best.characteristic(c.comp.criteria()) << ")"
            << " [equiv: " << best.equivalence_class().name() << "]"
            << " with CPUs " << string_util::join(best.cpus.cpulist(num_cpus), ",") << std::endl;

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

    void remap(int fd, const std::string& preferred_mapping_name)
    try {
        Client& c = _clients.at(fd);

        for (const auto& m : c.mappings) {
            if (m.name == preferred_mapping_name) {
                c.active_mapping = m;
                break;
            }
        }

        for (auto& t : c.threads) {
            std::cout << "Remapping: " << t.name << std::endl;

            cpu_set_t* mask = &t.affinity;
            CPU_ZERO(mask);

            if (c.dynamic_client) {
                std::cout << "Enabling all cpus for dynamic clients." << std::endl;

                for (auto [name, cpu] : c.active_mapping.thread_map) {
                    CPU_SET(cpu, mask);
                    std::cout << "Enabling cpu " << cpu << " (for thread " << name << ")" << std::endl;
                }
            } else {
                std::cout << "Pinning thread " << t.name << " to cpu " << c.active_mapping.thread_map.at(t.name) << std::endl;
                CPU_SET(c.active_mapping.thread_map.at(t.name), mask);
            }

            if (sched_setaffinity(t.tid, sizeof(cpu_set_t), mask) != 0)
                std::cerr << "Failed to set cpu affinity for the thread." << std::endl
                    << strerror(errno) << std::endl;
        }
    } catch (std::out_of_range&) {
        std::cerr << "Unknown client " << fd << std::endl;
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

                            c.comp = Client::Comp(string_util::strip(message.new_client_data.compare_criteria),
                                    message.new_client_data.compare_more_is_better);

                            if (message.new_client_data.has_filter_criteria)
                                c.filter = Filter(message.new_client_data.filter_criteria);

                            if (message.new_client_data.has_preferred_mapping) {
                                std::string preferred_mapping = string_util::strip(message.new_client_data.preferred_mapping);
                                c.active_mapping = use_preferred_mapping(c, preferred_mapping);
                            } else {
                                c.active_mapping = select_best_mapping(c);
                            }

                            std::cout << "New client registered: '" << exec << "' [" << pid << "]" << std::endl;
                            cpu_set_t mask = c.new_thread("@main", c.pid);
                            if (sched_setaffinity(c.pid, sizeof(cpu_set_t), &mask) != 0)
                                std::cerr << "Failed to set cpu affinity for main thread." << std::endl
                                    << strerror(errno) << std::endl;

                            std::cout << "Run client according to mapping " << c.active_mapping.name << "." << std::endl;
                            std::cout << "Client is " << ((c.dynamic_client) ? "managed dynamically by CFS." : "mapped statically.") << std::endl;

                            /* We will manage this client. */
                            managed = true;
                        } catch (std::out_of_range&) {
                            std::cout << "Unknown client: '" << exec << "' [" << pid << "]" << std::endl;
                            managed = false;
                        } catch (NoMappingError&) {
                            std::cout << "Couldn't find a proper mapping for client: '" << exec << "' [" << pid << "]" << std::endl;
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
                            cpu_set_t mask = c.new_thread(name, tid);
                            std::cout << "New thread '" << name << "' [" << tid << "] for client '" << c.exec << "'"
                                << " registered." << std::endl;

                            /* Set the affinity for the thread. */
                            if (sched_setaffinity(tid, sizeof(cpu_set_t), &mask) != 0)
                                std::cerr << "Failed to set cpu affinity for the thread." << std::endl
                                    << strerror(errno) << std::endl;

                            managed = true;
                        } catch (std::out_of_range) {
                            std::cout << "Unknown thread '" << name << "' for client '" << c.exec << "'" << std::endl;
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        TetrisData ack;
                        ack.op = TetrisData::NEW_THREAD_ACK;
                        ack.new_thread_ack_data.managed = managed;

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

    void print_mappings() {
        std::cout << "Currently active mappings:" << std::endl
                  << "==========================" << std::endl;
        for (const auto& [name, client] : _clients) {
            std::cout << "-> Client: " << name << std::endl;

            for (const auto& t : client.threads) {
                std::vector<std::string> cpus;
                std::cout << "Thread: (" << t.tid << "," << t.name << ") ";

                for (int i = num_cpus - 1; i >= 0; i--) {
                    std::cout << CPU_ISSET(i, &t.affinity);
                    if (CPU_ISSET(i, &t.affinity)) {
                        cpus.push_back(std::to_string(i));
                    }
                }

                std::cout << " [" << string_util::join(cpus, ',') << "]" << std::endl;
            }
        }
        std::cout << "======= END OF LIST =======" << std::endl;
    } 

    void update_mappings()
    {
        std::cout << "Update mapping database (" << _mappings_path << ")." << std::endl;
        _mappings.clear();

        try {
            path_util::for_each_file(_mappings_path, [&](const std::string& file) -> void {
                if (path_util::extension(file) == ".csv") {
                    std::string program = string_util::strip(path_util::filename(file));
                    std::cout << " -> found mapping for '" << program << "'" << std::endl;

                    _mappings.emplace(program, parse_mapping(file));
                }
            });
        } catch (std::exception& e) {
            std::cout << "Reading mappings failed with:" << std::endl << e.what() << std::endl;
        }
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

    /* Setting up control socket */
    Socket ctl_sock;
    int ctl_fd = -1;
    try {
        ctl_sock.open(CONTROL_SOCKET);
        ctl_sock.non_blocking();
        ctl_sock.listening();
        ctl_fd = ctl_sock.fd();
    } catch (std::runtime_error& e) {
        std::cerr << "Failed to open control socket" << std::endl
          << e.what() << std::endl;
        return 2;
    }

    std::cout << "Server socket initialized at " << server_sock.path() << " ("
        << sock_fd << ")." << std::endl;
    std::cout << "Control socket initialized at " << ctl_sock.path() << " ("
        << ctl_fd << ")." << std::endl;

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
        sigaddset(&sigmask, SIGUSR2);

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
        for (auto fd : {sock_fd, ctl_fd, sig_fd}) {
            e.data.fd = fd;
            e.events = EPOLLIN;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
                std::cerr << "Failed to add socket " << fd << " to epoll." << std::endl
                          << strerror(errno) << std::endl;
                return 1;
            }
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

            if (cur->data.fd == sock_fd || cur->data.fd == ctl_fd) {
                /* There are a new connections at the socket.
                 * Connect with all of them.
                 */
                while (1) {
                    sockaddr_un in_sock;
                    socklen_t in_sock_size = sizeof(in_sock);
                    int infd = ::accept(cur->data.fd, reinterpret_cast<sockaddr*>(&in_sock), &in_sock_size);
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
                    if (cur->data.fd == sock_fd) {
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
                    } else if (cur->data.fd == ctl_fd) {
                        ControlData cd;
                        Connection(infd, in_sock).read(cd);
                        switch (cd.op) {
                            case ControlData::REMAP_CLIENT:
                                std::cout << "Remapping client " << cd.remap_data.client_fd
                                          << " to mapping " << cd.remap_data.new_mapping << std::endl;
                                manager.remap(cd.remap_data.client_fd, string_util::strip(cd.remap_data.new_mapping));
                                break;
                            case ControlData::ERROR:
                              std::cerr << "An error happened in the control connection." << std::endl;
                              break;
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
                    } else if (siginfo.ssi_signo == SIGUSR2) {
                        std::cout << "Printing mappings ..." << std::endl;
                        manager.print_mappings();
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
