#ifndef __CONFIG_ARCHITECTURE_H__
#define __CONFIG_ARCHITECTURE_H__

#pragma once


#include <map>
#include <string>


static
std::map<std::string, int> cpu_map = 
{
    {"ARM00", 0},
    {"ARM01", 1},
    {"ARM02", 2},
    {"ARM03", 3},
    {"ARM04", 4},
    {"ARM05", 5},
    {"ARM06", 6},
    {"ARM07", 7}
};

#endif /* __CONFIG_ARCHITECTURE_H__ */
