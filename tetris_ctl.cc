#include "tetris.h"
#include <iostream>
#include "socket.h"
#include "connection.h"
#include <memory>

int main(int argc, char* argv[]) {
    char dummy[25];
    if (argc < 2) {
        std::cout << "Syntax: " << argv[0] << " <operation> <params>" << std::endl << std::endl;
        std::cout << "Where operation is one of: " << std::endl;
        std::cout << "remap - params: <fd> <mapping>" << std::endl;
        return 1;
    }
    if (argv[1] == std::string("remap")) {
        if (argc != 4) {
            std::cout << "Remap expects two arguments, fd and mapping" << std::endl;
            return 1;
        }
        auto c = std::make_unique<Connection>(CONTROL_SOCKET);
        ControlData d;
        d.op = ControlData::REMAP_CLIENT;
        d.remap_data.client_fd = atoi(argv[2]);
        strncpy(d.remap_data.new_mapping,argv[3],25);
        d.remap_data.new_mapping[24] = '\0';
        c->write(d);
    }
}
