#ifndef __ALGORITHM_H__
#define __ALGORITHM_H__

#pragma once


#include "cpulist.h"
#include "mapping.h"

#include <vector>


std::vector<Mapping> tetris_mappings(const std::vector<Mapping>& all_mappings, const CPUList& occupied_cpus);

#endif /* __ALGORITHM_H__ */
