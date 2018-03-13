#ifndef __TETRIS_H__
#define __TETRIS_H__

#pragma once


static const char* SERVER_SOCKET = "/tmp/tetris_socket";
static const char* CONTROL_SOCKET = "/tmp/tetris_ctl";


struct ControlData {
    enum Operations {
        REMAP_CLIENT = 1,
        ERROR
    };

    Operations op;
    union {
        struct {
            int client_fd;
            char new_mapping[25];
        } remap_data;
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
            bool has_preferred_mapping;
            bool dynamic_client;
            char preferred_mapping[25];
        } new_client_data;
        struct {
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
