#include "connection.h"
#include "tetris.h"

#include <algorithm>
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
 * TETRIS library support
 ***/

static
bool tetris_new_client(LockedConnection conn, int pid, const char* exec) {
    TetrisData data;

    /* Send the new-client message to the server. */
    data.op = TetrisData::NEW_CLIENT;
    data.new_client_data.pid = pid;
    std::strncpy(data.new_client_data.exec, exec, sizeof(data.new_client_data.exec));

    if (conn->write(data) != Connection::OutState::DONE) {
        perror("Failed to send new-client message.\n");
        return false;
    }

    /* Get the answer. */
    memset(&data, 0, sizeof(data));
    if (conn->read(data) != Connection::InState::DONE) {
        perror("Failed to get answer from server.\n");
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
        perror("Failed to send new-thread message.\n");
        return false;
    }

    /* Get the answer. */
    memset(&data, 0, sizeof(data));
    if (conn->read(data) != Connection::InState::DONE) {
        perror("Failed to get answer from server.\n");
        return false;
    }

    printf("Thread %s should run at %d\n", name, data.new_thread_ack_data.cpu);
    return data.new_thread_ack_data.managed;
}


/***
 * Library setup and tierdown
 ***/
bool managed_by_tetris;
std::unique_ptr<Connection> conn;

extern "C"
void __attribute__((constructor)) setup(void)
{
    printf("Loading TETRIS support\n");

    try {
        conn = std::make_unique<Connection>(SERVER_SOCKET);

        char exec[100];
        readlink("/proc/self/exe", exec, sizeof(exec));
        int pid = getpid();

        if (tetris_new_client(conn->locked(), pid, exec)) {
            printf("->> Managed by TETRIS <<-\n");
            managed_by_tetris = true;
        } else {
            printf("->> NOT managed by TETRIS <<-\n");
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

struct ThreadInfo {
    pthread_t*          pthread_id;
    pthread_mutex_t     mtx;
    pthread_cond_t      cond;

    pid_t               tid;
    char                name[100];

    bool                setup = false;
    bool                managed = false;

    void* (*func)(void*);
    void* arg;
};

std::vector<ThreadInfo*> threads;

static
void* thread_wrapper(void *arg)
{
    auto ti = static_cast<ThreadInfo*>(arg);

    pthread_mutex_lock(&ti->mtx);

    /* Update the tid information in the ThreadInfo struct for this thread. */
    ti->tid = syscall(SYS_gettid);

    if (!ti->setup) {
        /* Wait until the thread is properly setup. */
        pthread_cond_wait(&ti->cond, &ti->mtx);
    }

    ti->managed = tetris_new_thread(conn->locked(), ti->tid, ti->name);

    pthread_mutex_unlock(&ti->mtx);

    /* Call the actual function. */
    return ti->func(ti->arg);
}

extern "C"
int pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
        void* (*routine)(void*), void *arg) 
{
    using real_func_t = int (*)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);

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
            pthread_cond_init(&ti->cond, nullptr);

            ti->func = routine;
            ti->arg = arg;

            /* We need to safe the information so we can find it in
             * later pthread* calls. */
            threads.push_back(ti);

            return real_func(thread_id, attr, thread_wrapper, ti);
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_create function. */
            return real_func(thread_id, attr, routine, arg);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        perror("Failed to get real pthread_create function.\n");
        exit(-1);
    }
}

extern "C"
int pthread_setname_np(pthread_t thread_id, const char *name)
{
    using real_func_t = int(*)(pthread_t, const char*);

    /* Get the real pthread_setname_np function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_setname_np"));

    if (real_func != nullptr) {
        if (managed_by_tetris) {
            /* Search for the ThreadInfo struct of this thread. */
            auto iti = std::find_if(threads.begin(), threads.end(), [&](ThreadInfo* ti) -> bool {
                    return pthread_equal(*(ti->pthread_id), thread_id) != 0;
            });

            if (iti != threads.end()) {
                /* Ok we found the corresponding ThreadInfo. So first
                 * make the actual call and then signal the thread that
                 * it is properly setup now. */
                auto res = real_func(thread_id, name);

                auto ti = *iti;
                pthread_mutex_lock(&ti->mtx);
                strncpy(ti->name, name, sizeof(ti->name));
                ti->setup = true;

                pthread_cond_broadcast(&ti->cond);
                pthread_mutex_unlock(&ti->mtx);

                return res;
            } else {
                /* We could not find the corresponding ThreadInfo struct.
                 * Something went wrong here!. */
                perror("Failed to find appropriate ThreadInfo struct.\n");
                exit(-1);
            }
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_setname_np function. */
            return real_func(thread_id, name);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        perror("Failed to get real pthread_setname_np function.\n");
        exit(-1);
    }
}

extern "C"
int pthread_setaffinity_np(pthread_t thread_id, size_t cpusetsize,
        const cpu_set_t *cpuset)
{
    using real_func_t = int(*)(pthread_t, size_t, const cpu_set_t*);

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
        perror("Failed to get real pthread_setaffinity_np function.\n");
        exit(-1);
    }
}

