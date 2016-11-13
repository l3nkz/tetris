#include "connection.h"
#include "tetris.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


/***
 * Time Keeping
 ***/

template <typename T, typename Clock, typename Resolution>
class TimeKeeper
{
   private:
    T&      _total;
    typename Clock::time_point _start;
    bool _running;

   public:
    TimeKeeper(T& total) :
        _total{total}, _start{}, _running{false}
    {
        start();
    }

    ~TimeKeeper()
    {
        stop();
    }

    void start()
    {
        if (!_running) {
            _start = Clock::now();
            _running = true;
        }
    }

    void stop()
    {
        if (_running) {
            auto end = Clock::now();
            _total += std::chrono::duration_cast<Resolution>(end - _start).count();
            _running = false;
        }
    }
};


/***
 * Logging
 ***/

class Logger
{
   private:
    enum Level {
        NONE = 0,
        ERROR = 1,
        INFO = 2,
        DEBUG = 3
    };

    int _level;

   public:
    Logger() :
        _level(NONE)
    {
        char *log_level = getenv("TETRIS_LOGLEVEL");

        if (log_level) {
            if (strcmp(log_level, "DEBUG") == 0)
                _level = DEBUG;
            else if (strcmp(log_level, "INFO") == 0)
                _level = INFO;
            else if (strcmp(log_level, "ERROR") == 0)
                _level = ERROR;
        }
    }

    template <typename... Args>
    void debug(const char* fmt, Args... args)
    {
        if (_level >= DEBUG)
            printf(fmt, args...);
    }

    template <typename... Args>
    void info(const char* fmt, Args... args)
    {
        if (_level >= INFO)
            printf(fmt, args...);
    }

    template <typename... Args>
    void error(const char* fmt, Args... args)
    {
        if (_level >= ERROR)
            printf(fmt, args...);
    }

    template <typename... Args>
    void always(const char* fmt, Args... args)
    {
        printf(fmt, args...);
    }
};


/***
 * Thread management
 ***/

struct ThreadInfo {
    pthread_t*          pthread_id;
    pthread_mutex_t     mtx;

    pid_t               tid;
    char                name[100];

    bool                named = false;
    bool                ready = false;

    bool                managed = false;

    void* (*func)(void*);
    void* arg;
};


/***
 * Global variables
 ***/

using LoggerPtr = std::unique_ptr<Logger>;
using Timer = TimeKeeper<std::atomic_ulong, std::chrono::system_clock, std::chrono::nanoseconds>;
using ConnectionPtr = std::unique_ptr<Connection>;
using ThreadList = std::vector<ThreadInfo*>;
using ThreadListPtr = std::unique_ptr<ThreadList>;

LoggerPtr logger;
ConnectionPtr connection;
ThreadListPtr threads;

bool managed_by_tetris;
std::atomic_ulong time_ns;


/***
 * TETRIS library support
 ***/

static
bool tetris_new_client(LockedConnection conn, int pid, const char* exec, const char* preferred_mapping, bool dynamic_client) {
    TetrisData data;

    /* Send the new-client message to the server. */
    data.op = TetrisData::NEW_CLIENT;
    data.new_client_data.pid = pid;
    data.new_client_data.dynamic_client = dynamic_client;
    std::strncpy(data.new_client_data.exec, exec, sizeof(data.new_client_data.exec));
    if (preferred_mapping) {
        data.new_client_data.has_preferred_mapping = true;
        std::strncpy(data.new_client_data.preferred_mapping, preferred_mapping, sizeof(data.new_client_data.preferred_mapping));
    } else {
        data.new_client_data.has_preferred_mapping = false;
    }

    if (conn->write(data) != Connection::OutState::DONE) {
        logger->error("Failed to send new-client message.\n");
        return false;
    }

    /* Get the answer. */
    memset(&data, 0, sizeof(data));
    if (conn->read(data) != Connection::InState::DONE) {
        logger->error("Failed to get answer from server.\n");
        return false;
    }

    return data.new_client_ack_data.managed;
}

static
bool tetris_new_thread(LockedConnection conn, int tid, const char* name)
{
    TetrisData data;

    /* Send the new-thread message to the server. */
    data.op = TetrisData::NEW_THREAD;
    data.new_thread_data.tid = tid;
    std::strncpy(data.new_thread_data.name, name, sizeof(data.new_thread_data.name));

    if (conn->write(data) != Connection::OutState::DONE) {
        logger->error("Failed to send new-thread message.\n");
        return false;
    }

    /* Get the answer. */
    memset(&data, 0, sizeof(data));
    if (conn->read(data) != Connection::InState::DONE) {
        logger->error("Failed to get answer from server.\n");
        return false;
    }

    logger->info("Thread %s should run at %d\n", name, data.new_thread_ack_data.cpu);
    return data.new_thread_ack_data.managed;
}


/***
 * Library setup and tierdown
 ***/

extern "C"
void __attribute__((constructor)) setup(void)
{
    Timer t{time_ns};

    time_ns = 0;
    logger = std::make_unique<Logger>();
    threads = std::make_unique<ThreadList>();

    logger->info("Loading TETRIS support\n");

    try {
        connection = std::make_unique<Connection>(SERVER_SOCKET);

        char exec[100];
        memset(exec, 0, sizeof(exec));

        readlink("/proc/self/exe", exec, sizeof(exec));
        int pid = getpid();
        char *preferred_mapping = getenv("TETRIS_PREFERRED_MAPPING");
	char *dynamic_mapping = getenv("TETRIS_DYNAMIC_MAPPING");
	bool dynamic_client = false;
	if (dynamic_mapping && strcmp(dynamic_mapping,"1") == 0) {
		logger->info("Enabled dynamic/CFS mappings!");
		dynamic_client = true;
	}

        if (tetris_new_client(connection->locked(), pid, exec, preferred_mapping, dynamic_client)) {
            logger->info("->> Managed by TETRIS <<-\n");
            managed_by_tetris = true;
        } else {
            logger->info("->> NOT managed by TETRIS <<-\n");
            managed_by_tetris = false;
            connection.release();
        }
    } catch(std::runtime_error& e) {
        logger->error("Failed to connect to TETRIS server.\n--> %s <--\n", e.what());
        managed_by_tetris = false;
        connection.release();
    }
}

extern "C"
void __attribute__((destructor)) tierdown(void)
{
    Timer t{time_ns};

    if (managed_by_tetris)
        connection.release();

    t.stop();

    unsigned long _ns = time_ns;
    unsigned long ms = _ns/1000000;
    unsigned us = (_ns % 1000000) / 1000;
    unsigned ns = _ns % 1000;
    logger->always("Total time spent in TETRIS: %lu.%03u%03lu ms (%lu ns)\n", ms, us, ns, _ns);
}


/***
 * pthread wrapper
 ***/

static
void* thread_wrapper(void *arg)
{
    Timer t{time_ns};

    auto ti = static_cast<ThreadInfo*>(arg);

    pthread_mutex_lock(&ti->mtx);

    /* Update the tid information in the ThreadInfo struct for this thread. */
    ti->tid = syscall(SYS_gettid);
    ti->ready = true;

    if (ti->named && ti->ready)
        ti->managed = tetris_new_thread(connection->locked(), ti->tid, ti->name);

    pthread_mutex_unlock(&ti->mtx);

    /* Call the actual function. */
    t.stop();
    return ti->func(ti->arg);
}

extern "C"
int pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
        void* (*routine)(void*), void *arg) 
{
    using real_func_t = int (*)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);

    Timer t{time_ns};

    /* Get the real pthread_create function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_create"));

    if (real_func != nullptr) {
        if (managed_by_tetris) {
            /* This program is managed by TETRIS. Accordingly create the
             * thread and wait until a name is assigned to it so that
             * the TETRIS server can move this thread to the appropriate
             * CPU. */

            /* We need to create the ThreadInfo struct for this thread and
             * call the wrapper function which will perform all the necessary
             * setup with the TETRIS server. */
            auto ti = new ThreadInfo{};
            ti->pthread_id = thread_id;
            pthread_mutex_init(&ti->mtx, nullptr);

            ti->func = routine;
            ti->arg = arg;

            /* We need to safe the information so we can find it in
             * later pthread* calls. */
            threads->push_back(ti);

            return real_func(thread_id, attr, thread_wrapper, ti);
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_create function. */
            return real_func(thread_id, attr, routine, arg);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_create function.\n");
        exit(-1);
    }
}

extern "C"
int pthread_setname_np(pthread_t thread_id, const char *name)
{
    using real_func_t = int(*)(pthread_t, const char*);

    Timer t{time_ns};

    /* Get the real pthread_setname_np function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_setname_np"));

    if (real_func != nullptr) {
        if (managed_by_tetris) {
            /* Search for the ThreadInfo struct of this thread. */
            auto iti = std::find_if(threads->begin(), threads->end(), [&](ThreadInfo* ti) -> bool {
                    return pthread_equal(*(ti->pthread_id), thread_id) != 0;
            });

            if (iti != threads->end()) {
                /* Ok we found the corresponding ThreadInfo. So first
                 * make the actual call and then signal the thread that
                 * it is properly setup now. */
                auto res = real_func(thread_id, name);

                auto ti = *iti;
                pthread_mutex_lock(&ti->mtx);
                strncpy(ti->name, name, sizeof(ti->name));
                ti->named = true;

                if (ti->named && ti->ready)
                    ti->managed = tetris_new_thread(connection->locked(), ti->tid, ti->name);

                pthread_mutex_unlock(&ti->mtx);

                return res;
            } else {
                /* We could not find the corresponding ThreadInfo struct.
                 * Something went wrong here!. */
                logger->error("Failed to find appropriate ThreadInfo struct.\n");
                exit(-1);
            }
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_setname_np function. */
            return real_func(thread_id, name);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_setname_np function.\n");
        exit(-1);
    }
}

extern "C"
int pthread_setaffinity_np(pthread_t thread_id, size_t cpusetsize,
        const cpu_set_t *cpuset)
{
    using real_func_t = int(*)(pthread_t, size_t, const cpu_set_t*);

    Timer t{time_ns};

    /* Get the real pthread_setaffinity_np function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_setaffinity_np"));

    if (real_func != nullptr) {
        if (managed_by_tetris) {
            /* This program is managed by TETRIS. The TETRIS server
             * decides where to place this thread. So just ignore this
             * request. */
            return 0;
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_setaffinity_np function. */
            return real_func(thread_id, cpusetsize, cpuset);
        }

    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_setaffinity_np function.\n");
        exit(-1);
    }
}

