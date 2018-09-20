#ifndef __TETRIS_H__
#define __TETRIS_H__

#pragma once


#include <sched.h>


static const char* SERVER_SOCKET = "/tmp/tetris_socket";
static const char* CONTROL_SOCKET = "/tmp/tetris_ctl";


struct ControlData {
    enum Operations {
        UPDATE_CLIENT = 1,
        UPDATE_MAPPINGS = 2,
        BLOCK_CPUS = 3,
        ERROR
    };

    Operations op;
    union {
        struct {
            int client_fd;
            bool has_dynamic_client;
            bool dynamic_client;
            bool has_compare_criteria;
            char compare_criteria[25];
            bool compare_more_is_better;
            bool has_preferred_mapping;
            char preferred_mapping[25];
            bool has_filter_criteria;
            char filter_criteria[50];
        } update_data;
        struct {
            cpu_set_t cpus;
        } block_cpus_data;
    };
};

struct TetrisData {
    enum Operations {
        NEW_CLIENT = 1,
        NEW_CLIENT_ACK = 2,
        NEW_THREAD = 3,
        NEW_THREAD_ACK = 4,
        ERROR
    };

    Operations op;
    union {
        struct {
            int pid;
            char exec[100];
            bool dynamic_client;
            char compare_criteria[25];
            bool compare_more_is_better;
            bool has_preferred_mapping;
            char preferred_mapping[25];
            bool has_filter_criteria;
            char filter_criteria[50];
        } new_client_data;
        struct {
            int id;
            bool managed;
        } new_client_ack_data;
        struct {
            int tid;
            char name[100];
        } new_thread_data;
        struct {
            bool managed;
        } new_thread_ack_data;
    };
};

#endif /* __TETRIS_H__ */
