#include "connection.h"
#include "tetris.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>


/***
 * TETRIS library support
 ***/

bool managed_by_tetris;
std::unique_ptr<Connection> conn;

static
bool register_client(Connection& conn) {
    TetrisData data;

    data.op = TetrisData::NEW_CLIENT;
    data.new_client_data.pid = getpid();
    readlink("/proc/self/exe", data.new_client_data.exec, sizeof(data.new_client_data.exec));

    conn.write(data);

    return true;
}

extern "C"
void __attribute__((constructor)) setup(void)
{
    printf("Loading TETRIS support.\n");

    try {
        conn = std::make_unique<Connection>(SERVER_SOCKET);
        if (register_client(*conn)) {
            managed_by_tetris = true;
        } else {
            managed_by_tetris = false;
            conn.release();
        }
    } catch(std::runtime_error& e) {
        printf("Failed to connect to TETRIS server.\n--> %s <--\n", e.what());
        managed_by_tetris = false;
        conn.release();
    }
}

extern "C"
void __attribute__((destructor)) tierdown(void)
{
    if (managed_by_tetris)
        conn.release();
}


/***
 * pthread wrapper
 ***/

struct ThreadFunc {
    void* (*func)(void*);
    void* arg;
};

static
void* thread_wrapper(void *arg)
{
    auto tf = static_cast<ThreadFunc*>(arg);
    void* (*func)(void*) = tf->func;
    void* func_arg = tf->arg;
    delete tf;

    /* Wait until the TETRIS server signals us to continue running. */
    kill(getpid(), SIGSTOP);

    return func(func_arg);
}

extern "C"
int pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
        void* (*routine)(void*), void *arg) 
{
    using real_func_t = int (*)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);

    /* TODO: Connect to TETRIS server and give information about new thread. */

    real_func_t real_func = NULL;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_create"));

    if (real_func != NULL) {
        /* This struct will be deleted in the thread wrapper function. */
        auto tf = new ThreadFunc{};
        tf->func = routine;
        tf->arg = arg;

        return real_func(thread_id, attr, thread_wrapper, tf);
    } else {
        exit(1);
    }
}

extern "C"
int pthread_setname_np(pthread_t thread_id, const char *name)
{
    /* TODO: Connect to TETRIS server and give information about thread name. */
    return -1;
}

extern "C"
int pthread_setaffinity_np(pthread_t thread_id, size_t cpusetsize,
        const cpu_set_t *cpuset)
{
    /* TODO: Connect to TETRIS server and make the server change the affinity. */
    return -1;
}

