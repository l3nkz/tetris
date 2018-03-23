#include "algorithm.h"
#include "connection.h"
#include "csv.h"
#include "debug_util.h"
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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>



/***
 * Global variables
 ***/

using ConnectionPtr = std::shared_ptr<Connection>;

const static int MAXEVENTS = 100;
debug::LoggerPtr logger;


/***
 * Failure handling for no mapping found
 ***/

class NoMappingError : public std::runtime_error
{
   public:
    using std::runtime_error::runtime_error;
};


/***
 * Client Program
 ***/

class Client
{
   public:
    struct Thread
    {
        std::string     name;
        int             tid;
        CPUList         cpus;

        Thread(const std::string& name, int tid, CPUList cpus) :
            name{name}, tid{tid}, cpus{cpus}
        {}
    };

    class Comp
    {
       private:
        std::string     _criteria;
        bool            _more_is_better;
        std::function<bool(const double, const double)> _comp;

       public:
        Comp(const std::string compare_criteria, bool compare_more_is_better) :
            _criteria{compare_criteria}, _more_is_better{compare_more_is_better}
        {
            if (_more_is_better)
                _comp = std::greater<double>{};
            else
                _comp = std::less<double>{};
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

        std::string repr() const
        {
            std::stringstream ss;
            ss << _criteria << "(" << (_more_is_better ? ">" : "<") << ")";

            return ss.str();
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
        logger->info("New client created\n");
    }

    ~Client()
    {
        logger->info("Client removed\n");
    }

    CPUList cpus() const
    {
        return active_mapping.cpus;
    }

    void update_mapping(const Mapping& new_mapping)
    {
        if (new_mapping.name == active_mapping.name)
            return;

        logger->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid, new_mapping.name.c_str());
        active_mapping = new_mapping;

        for (auto& t : threads) {
            CPUList cpus;
            if (dynamic_client)
                cpus = active_mapping.cpus;
            else
                cpus = active_mapping.cpu(t.name);

            logger->debug(" * remap thread '%s' [%i] from cpu(s) %s to cpu(s) %s\n", t.name.c_str(), t.tid,
                    string_util::join(t.cpus.cpulist(num_cpus), ",").c_str(),
                    string_util::join(cpus.cpulist(num_cpus), ",").c_str());

            t.cpus = cpus;

            cpu_set_t mask = cpus.cpu_set();
            if (sched_setaffinity(t.tid, sizeof(cpu_set_t), &mask) != 0)
                logger->warning("Failed to set cpu affinity for thread '%s': %s\n", t.name.c_str(), strerror(errno));
        }
    }

    void new_thread(const std::string& name, int pid)
    {
        logger->info("New thread '%s' [%i] registered for client '%s'\n", name.c_str(), pid, exec.c_str());

        CPUList cpus;
        if (dynamic_client) {
            cpus = active_mapping.cpus;
            logger->debug(" * enabled cpu(s) %s (dynamic client)\n", string_util::join(cpus.cpulist(num_cpus), ",").c_str());
        } else {
            cpus = active_mapping.cpu(name);
            logger->debug(" * enabled cpu(s) %s\n", string_util::join(cpus.cpulist(num_cpus), ",").c_str());
        }

        auto it = std::find_if(threads.begin(), threads.end(), [&](const auto& t) { return t.name == name; });
        if (it == threads.end()) {
            threads.emplace_back(name, pid, cpus);

            cpu_set_t mask = cpus.cpu_set();
            if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) != 0)
                logger->warning("Failed to set cpu affinity for thread '%s': %s\n", name.c_str(), strerror(errno));
        } else
            logger->warning("Duplicate thread '%s'\n", name.c_str());
    }
};


/***
 * Client Manager
 ***/

class Manager
{
   private:
    std::map<int, Client>   _clients;
    std::string             _mappings_path;
    std::map<std::string, std::vector<Mapping>> _mappings;

    std::vector<Mapping> parse_mapping(const std::string& file)
    {
        CSVData data{file};
        std::vector<Mapping> mappings;

        for (const auto& row : data.row_iter()) {
            std::vector<std::pair<std::string, std::string>> threads;
            std::vector<std::pair<std::string, std::string>> characteristics;

            for (const auto& col : row.names()) {
                if (string_util::starts_with(col, "t_")) {
                    /* Columns starting with 't_' are interpreted as threads */
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

            mappings.emplace_back(name, threads, characteristics);
        }

        return mappings;
    }

    Mapping select_best_mapping(Client& c)
    {
        logger->info("Search for best mapping for '%s' [%d] using criteria %s\n", c.exec.c_str(), c.pid, c.comp.repr().c_str());

        CPUList occupied_cpus;
        for (const auto& [name, cl] : _clients) {
            occupied_cpus |= cl.cpus();
        }

        if (occupied_cpus.nr_cpus() == 0)
            logger->debug(" * Already taken cpu(s): none\n");
        else
            logger->debug(" * Already taken cpu(s): %s\n", string_util::join(occupied_cpus.cpulist(num_cpus), ",").c_str());

        /* Get all the TETRiS mappings for this client */
        auto possible_mappings = tetris_mappings(c.mappings, occupied_cpus);
        if (possible_mappings.empty())
            throw NoMappingError("Can't find a proper mapping for the client");
        else
            logger->debug(" * There are %i mapping(s) for this client\n", possible_mappings.size());

        /* Now select the best one of the available ones according to the given
         * criteria and the given filter option */
        auto comp = [&c] (const Mapping& other, const Mapping& best) -> bool {
            return c.comp(other, best);
        };
        auto filter = [&c] (const Mapping& m) -> bool {
            return c.filter(m);
        };

        auto best = possible_mappings.front();
        for (const auto& m : possible_mappings) {
            if (comp(m, best) && filter(m)) {
                logger->debug(" * Found better mapping: %s (%f@%s) [%s] vs %s (%f@%s) [%s]\n", m.name.c_str(),
                                                                          m.characteristic(c.comp.criteria()),
                                                                          c.comp.repr().c_str(),
                                                                          m.equivalence_class().name().c_str(),
                                                                          best.name.c_str(),
                                                                          best.characteristic(c.comp.criteria()),
                                                                          c.comp.repr().c_str(),
                                                                          best.equivalence_class().name().c_str());
                best = m;
            }
        }

        logger->info("The best mapping: %s (%f@%s) [%s]\n", best.name.c_str(), best.characteristic(c.comp.criteria()),
                                                            c.comp.repr().c_str(), best.equivalence_class().name().c_str());

        return best;
    }

    Mapping use_preferred_mapping(Client& c, const std::string& preferred_mapping_name)
    {
        logger->info("Use preferred mapping '%s' for '%s' [%d]\n", preferred_mapping_name.c_str(), c.exec.c_str(), c.pid);

        auto it = std::find_if(c.mappings.begin(), c.mappings.end(), [&](const auto& m) { return m.name == preferred_mapping_name; });
        if (it != c.mappings.end())
            return *it;
        else {
            logger->info("Couldn't find preferred mapping\n");
            return select_best_mapping(c);
        }
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

        logger->info("Change mapping for client '%s' [%d] to mapping %s\n", c.exec.c_str(), c.pid, preferred_mapping_name.c_str());

        auto it = std::find_if(c.mappings.begin(), c.mappings.end(), [&](const auto& m) { return m.name == preferred_mapping_name; });
        if (it == c.mappings.end()) {
            logger->info("Unknown mapping %s for client %i\n", preferred_mapping_name.c_str(), fd);
            return;
        } else {
            logger->info("Changing mapping for client '%s' [%d] to mapping %s\n", c.exec.c_str(), c.pid, preferred_mapping_name.c_str());
            c.update_mapping(*it);
        }
    } catch (std::out_of_range&) {
        logger->error("Unknown client %i\n", fd);
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
                            logger->info("New client registered: '%s' [%d]\n", exec.c_str(), pid);

                            /* Update the client data. */
                            c.pid = pid;
                            c.exec = exec;
                            c.dynamic_client = message.new_client_data.dynamic_client;
                            c.mappings = _mappings.at(exec);

                            c.comp = Client::Comp(string_util::strip(message.new_client_data.compare_criteria),
                                    message.new_client_data.compare_more_is_better);

                            logger->info(" * criteria: %s\n", c.comp.repr().c_str());

                            if (message.new_client_data.has_filter_criteria)
                                c.filter = Filter(message.new_client_data.filter_criteria);

                            logger->info(" * filter: %s\n", c.filter.repr().c_str());

                            if (message.new_client_data.has_preferred_mapping) {
                                std::string preferred_mapping = string_util::strip(message.new_client_data.preferred_mapping);
                                c.update_mapping(use_preferred_mapping(c, preferred_mapping));
                            } else {
                                c.update_mapping(select_best_mapping(c));
                            }

                            logger->info(" * mapping: %s (%f@%s) [%s]\n", c.active_mapping.name.c_str(),
                                                                          c.active_mapping.characteristic(c.comp.criteria()),
                                                                          c.comp.repr().c_str(),
                                                                          c.active_mapping.equivalence_class().name().c_str());
                            logger->info(" * thread placement: %s\n", c.dynamic_client ? "CFS" : "static");

                            /* Add the main thread to the client */
                            c.new_thread("@main", c.pid);

                            /* We will manage this client. */
                            managed = true;
                        } catch (std::out_of_range&) {
                            logger->error("Unknown client: '%s' [%i]\n", exec.c_str(), pid);
                            managed = false;
                        } catch (NoMappingError&) {
                            logger->warning("Couldn't find a proper mapping for client: '%s' [%i]\n", exec.c_str(), pid);
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        TetrisData ack;
                        ack.op = TetrisData::NEW_CLIENT_ACK;
                        ack.new_client_ack_data.managed = managed;

                        if (conn->write(ack) != Connection::OutState::DONE) {
                            logger->error("Failed to acknowledge the new-client message\n");
                            managed = false;
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
                            c.new_thread(name, tid);
                            managed = true;
                        } catch (std::out_of_range) {
                            logger->error("Unknown thread: '%s' [%i] for client '%s'\n", name.c_str(), tid, c.exec.c_str());
                            managed = false;
                        }

                        /* We need to acknowledge this message. */
                        TetrisData ack;
                        ack.op = TetrisData::NEW_THREAD_ACK;
                        ack.new_thread_ack_data.managed = managed;

                        if (conn->write(ack) != Connection::OutState::DONE)
                            logger->error("Failed to acknowledge the new-thread message\n");

                        break;
                    }
                    default:
                        logger->warning("Other message received\n");
                }
            }
        }

        return close;
    } catch (std::out_of_range) {
        logger->warning("Received message for unknown client %i\n", fd);
        return true;
    }

    void print_mappings() {
        std::cout << "Currently active mappings:" << std::endl
                  << "==========================" << std::endl;
        for (const auto& [name, client] : _clients) {
            std::cout << "Client: " << name << std::endl;

            for (const auto& t : client.threads)
                std::cout << "-> Thread: (" << t.tid << "," << t.name << ") ["
                    << string_util::join(t.cpus.cpulist(num_cpus), ",") << "]" << std::endl;
        }
        std::cout << "======= END OF LIST =======" << std::endl;
    } 

    void update_mappings()
    {
        logger->info("Update mapping database (%s).\n", _mappings_path.c_str());
        _mappings.clear();

        try {
            path_util::for_each_file(_mappings_path, [&](const std::string& file) -> void {
                if (path_util::extension(file) == ".csv") {
                    std::string program = string_util::strip(path_util::filename(file));
                    logger->info(" -> found mapping for '%s'\n", program.c_str());

                    _mappings.emplace(program, parse_mapping(file));
                }
            });
        } catch (std::exception& e) {
            logger->error("Reading mappings failed with: %s\n", e.what());
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

    std::cout << "Welcome to TETRiS" << std::endl;

    /* Setup logging */
    logger = debug::Logger::get();

    /* Setting up the manager */
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
        return 1;
    }

    logger->info(" * Server socket: %s (%i)\n", server_sock.path(), sock_fd);
    logger->info(" * Control socket: %s (%i)\n", ctl_sock.path(), ctl_fd);

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
                            logger->error("An error happened while accepting a connection: %s", strerror(errno));
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
                            logger->error("Failed to add new connection to epoll: %s", strerror(errno));
                            ::close(infd);
                        } else {
                            logger->info("A new client connected (%i)\n", infd);

                            manager.client_connect(infd, in_conn);
                        }
                    } else if (cur->data.fd == ctl_fd) {
                        ControlData cd;
                        Connection(infd, in_sock).read(cd);

                        switch (cd.op) {
                            case ControlData::Operations::REMAP_CLIENT: {
                                int cfd = cd.remap_data.client_fd;
                                std::string new_mapping = string_util::strip(cd.remap_data.new_mapping);

                                logger->info("Change mapping of client %i to %s\n", cfd, new_mapping);
                                manager.remap(cfd, new_mapping);
                                break;
                            }
                            default:
                                logger->warning("Other message received.\n");
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
                        if (errno != EAGAIN)
                            logger->error("An error happened while reading data from signal fd: %s", strerror(errno));

                        break;
                    }

                    logger->info("Received a signal (%i)\n", siginfo.ssi_signo);

                    switch (siginfo.ssi_signo) {
                        case SIGUSR1:
                            manager.update_mappings();
                            break;
                        case SIGUSR2:
                            manager.print_mappings();
                            break;
                        default:
                            done = 1;
                    }
                }
            } else if (cur->events & EPOLLIN) {
                /* Some client tried to send us data. */
                logger->debug("The client sent a message\n");

                if (manager.client_message(cur->data.fd)) {
                    logger->info("The client disconnected\n");
                    manager.client_disconnect(cur->data.fd);
                }
            } else if (cur->events & EPOLLHUP) {
                /* Some client disconnected. */
                logger->info("The client disconnected\n");
                manager.client_disconnect(cur->data.fd);
            } else {
                logger->warning("Strange event at %i\n", cur->data.fd);
                ::close(cur->data.fd);
            }
        }
    }

    std::cout << "Exiting" << std::endl;
    ::close(sig_fd);

    return 0;
}
