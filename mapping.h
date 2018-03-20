#ifndef __MAPPING_H__
#define __MAPPING_H__

#pragma once


#include "cpulist.h"
#include "config.h"
#include "equivalence.h"


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

} /* Anonymous namespace */


class Mapping
{
   public:
    std::string     name;
    std::map<std::string, int> thread_map;
    std::map<std::string, double> characteristics_map;
    CPUList         cpus;

   private:
    Mapping(const Mapping& base, const std::map<int, int>& conv_map) :
        name{base.name}, thread_map{}, characteristics_map{base.characteristics_map}, cpus{}
    {
        for (const auto& [name, orig_cpu] : base.thread_map) {
            if (conv_map.find(orig_cpu) != conv_map.end()) {
                thread_map.emplace(name, conv_map.at(orig_cpu));
                cpus.set(conv_map.at(orig_cpu));
            } else {
                thread_map.emplace(name, orig_cpu);
                cpus.set(orig_cpu);
            }
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

        for (const auto& equiv : equivalences)  {
            if (equiv.is_in_equalence_class(cpus)) {
                std::vector<Mapping> result;

                for (const auto& conv_map : equiv.equivalent_mappings(cpus)) {
                    result.push_back(Mapping{*this, conv_map});
                }

                return result;
            }
        }

        throw std::runtime_error("Can't determine the mapping's equivalence class.");
    }

    const Equivalence& equivalence_class() const
    {
        for (const auto& equiv : equivalences) {
            if (equiv.is_in_equalence_class(cpus))
                return equiv;
        }

        throw std::runtime_error("Can't determine the mapping's equivalence class.");
    }
};

#endif /* __MAPPING_H__ */
