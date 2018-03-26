#include "connection.h"
#include "tetris.h"
#include "socket.h"

#include <iostream>
#include <memory>
#include <string>

#include <cstdlib>
#include <cstring>


void usage_upd_client()
{
    std::cout << "usage: tetrisctl upd_client [-h] ID" << std::endl
        << std::endl
        << "Options:" << std::endl
        << "   -h, --help           show this help message" << std::endl
        << std::endl
        << "Positionals:" << std::endl
        << " ID                     the id of the client managed by TETRiS" << std::endl
        << std::endl
        << "Parsed environment variables:" << std::endl
        << " TETRIS_DYNAMIC_CLIENT" << std::endl
        << " TETRIS_PREFERRED_MAPPING" << std::endl
        << " TETRIS_COMPARE_CRITERIA" << std::endl
        << " TETRIS_COMPARE_MORE_IS_BETTER" << std::endl
        << " TETRIS_FILTER_CRITERIA" << std::endl;
}

int op_update_client(int argc, char* argv[])
try {
    int client_id;

    if (argc != 3) {
        usage_upd_client();
        return 1;
    } else if (argc == 3) {
        std::string arg{argv[2]};

        if (arg == "-h" || arg == "--help") {
            usage_upd_client();
            return 0;
        } else {
            try {
                client_id = std::stoi(arg);
            } catch (std::exception) {
                std::cout << "Unknown option: " << arg << std::endl;
                usage_upd_client();
                return 1;
            }
        }
    }

    char *mapping_type = getenv("TETRIS_MAPPING_TYPE");

    char *compare_criteria = getenv("TETRIS_COMPARE_CRITERIA");
    bool compare_more_is_better = false;
    if (getenv("TETRIS_COMPARE_MORE_IS_BETTER"))
        compare_more_is_better = true;

    char *preferred_mapping = getenv("TETRIS_PREFERRED_MAPPING");

    char *filter_criteria = getenv("TETRIS_FILTER_CRITERIA");

    /* Connect to the server and transmit the data */
    auto conn = std::make_unique<Connection>(CONTROL_SOCKET);
    ControlData cd;

    cd.op = ControlData::Operations::UPDATE_CLIENT;
    cd.update_data.client_fd = client_id;

    if (mapping_type) {
        cd.update_data.has_dynamic_client = true;
        if (strcmp(mapping_type, "DYNAMIC") == 0)
            cd.update_data.dynamic_client = true;
        else
            cd.update_data.dynamic_client = false;
    }
 
    if (compare_criteria) {
        cd.update_data.has_compare_criteria = true;
        std::strncpy(cd.update_data.compare_criteria, compare_criteria, sizeof(cd.update_data.compare_criteria));
        cd.update_data.compare_more_is_better = compare_more_is_better;
    } else
        cd.update_data.has_compare_criteria = false;


    if (preferred_mapping) {
        cd.update_data.has_preferred_mapping = true;
        std::strncpy(cd.update_data.preferred_mapping, preferred_mapping, sizeof(cd.update_data.preferred_mapping));
    } else
        cd.update_data.has_preferred_mapping = false;

    if (filter_criteria) {
        cd.update_data.has_filter_criteria = true;
        std::strncpy(cd.update_data.filter_criteria, filter_criteria, sizeof(cd.update_data.filter_criteria));
    } else
        cd.update_data.has_filter_criteria = false;

    conn->write(cd);

    return 0;
} catch (std::runtime_error& e) {
    std::cout << "Something went wrong: " << e.what() << std::endl;
    return 1;
}

void usage_upd_mappings()
{

}

int op_update_mappings(int argc, char* argv[])
try {
   if (argc > 3) {
        usage_upd_mappings();
        return 1;
    } else if (argc == 3) {
        std::string arg{argv[2]};

        if (arg == "-h" || arg == "--help") {
            usage_upd_mappings();
            return 0;
        } else {
            std::cout << "Unknown option: " << arg << std::endl;
            usage_upd_mappings();
            return 1;
        }
    }

    /* Connect to the server and transmit the data */
    auto conn = std::make_unique<Connection>(CONTROL_SOCKET);
    ControlData cd;

    cd.op = ControlData::Operations::UPDATE_MAPPINGS;

    conn->write(cd);

    return 0;
} catch (std::runtime_error& e) {
    std::cout << "Something went wrong: " << e.what() << std::endl;
    return 1;
}

void usage()
{
    std::cout << "usage: tetrisctl [-h] OPERATION" << std::endl
        << std::endl
        << "Options:" << std::endl
        << "   -h, --help           show this help message" << std::endl
        << std::endl
        << "Operations:" << std::endl
        << "   upd_client           update a client's properties" << std::endl
        << "   upd_mappings         update the server's mapping database" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    std::string op{argv[1]};

    if (op == "-h" || op == "--help") {
        usage();
        return 0;
    } else if (op == "upd_client") {
        return op_update_client(argc, argv);
    } else if (op == "upd_mappings") {
        return op_update_mappings(argc, argv);
    } else {
        std::cout << "Unknown operation: " << op << std::endl;
        usage();
        return 1;
    }

    return 0;
}
