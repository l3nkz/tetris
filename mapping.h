#ifndef __MAPPING_H__
#define __MAPPING_H__

#pragma once


#include "cpulist.h"
#include "config.h"


#include <map>
#include <string>
#include <vector>

#include <sched.h>


namespace {

int cpu_nr_for_name(const std::string& name)
{
    auto i = cpu_map.find(name);
    if (i != cpu_map.end())
        return i->second;
    else
        return 0;
}

std::vector<CPUList> cpulists_equivalent_to(const CPUList& cpus)
{
    for (auto& equiv_list : equivalent_cpus) {
        if (std::find(equiv_list.begin(), equiv_list.end(), cpus) != equiv_list.end())
            return equiv_list;
    }

    throw std::runtime_error{"Can't find given cpu list in equivalent_cpus list."};
}

} /* Anonymous namespace */


class Mapping
{
   public:
    std::string     name;
    std::map<std::string, int> thread_map;
    double          exec_time;
    double          energy;
    double          memory;
    CPUList         cpus;

   private:
    Mapping(Mapping& base, const CPUList& cpus) :
        name{base.name}, thread_map{}, exec_time{base.exec_time}, energy{base.energy},
        memory{base.memory}, cpus{cpus}
    {
        auto conv_map = base.cpus.convert_map(cpus);

        for (const auto& t : base.thread_map) {
            thread_map.emplace(t.first, conv_map[t.second]);
        }
    }

   public:
    Mapping() = default;

    Mapping(const std::string& name, const std::vector<std::pair<std::string, std::string>>& threads,
            double exec_time, double energy, double memory) :
        name{name}, thread_map{}, exec_time{exec_time}, energy{energy}, memory{memory}, cpus{}
    {
        for (const auto& t : threads) {
            thread_map.emplace(t.first, cpu_nr_for_name(t.second));
            cpus.set(cpu_nr_for_name(t.second));
        }
    }

    std::vector<Mapping> equivalent_mappings()
    {
        std::vector<Mapping> result;

        for (const auto& cpulist : cpulists_equivalent_to(cpus)) {
            result.push_back(Mapping{*this, cpulist});
        }

        return result;
    }
};

#endif /* __MAPPING_H__ */
