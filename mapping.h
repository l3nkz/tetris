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
    std::map<std::string, double> characteristics_map;
    CPUList         cpus;

   private:
    Mapping(const Mapping& base, const CPUList& cpus) :
        name{base.name}, thread_map{}, characteristics_map{base.characteristics_map}, cpus{cpus}
    {
        auto conv_map = base.cpus.convert_map(cpus);

        for (const auto& t : base.thread_map) {
            thread_map.emplace(t.first, conv_map[t.second]);
        }
    }

   public:
    Mapping() = default;

    Mapping(const std::string& name, const std::vector<std::pair<std::string, std::string>>& threads,
            const std::vector<std::pair<std::string, std::string>>& characteristics) :
        name{name}, thread_map{}, characteristics_map{}, cpus{}
    {
        for (const auto& t : threads) {
            thread_map.emplace(t.first, cpu_nr_for_name(t.second));
            cpus.set(cpu_nr_for_name(t.second));
        }

        for (const auto& c : characteristics) {
            characteristics_map.emplace(c.first, std::stod(c.second));
        }
    }

    cpu_set_t cpu_mask(const std::string& thread) const
    {
        cpu_set_t mask;
        CPU_ZERO(&mask);

        if (thread_map.find(thread) != thread_map.end()) {
            CPU_SET(thread_map.at(thread), &mask);
        } else {
            /* If we don't know this thread we will enable all cores of this mapping */
            mask = cpus.cpu_set();
        }

        return mask;
    }

    double characteristic(const std::string& criteria) const
    {
        if (characteristics_map.find(criteria) != characteristics_map.end())
            return characteristics_map.at(criteria);

        throw std::runtime_error("Unknown characteristic criteria");
    }

    std::vector<Mapping> equivalent_mappings() const
    {
        std::vector<Mapping> result;

        for (const auto& cpulist : cpulists_equivalent_to(cpus)) {
            result.push_back(Mapping{*this, cpulist});
        }

        return result;
    }
};

#endif /* __MAPPING_H__ */
