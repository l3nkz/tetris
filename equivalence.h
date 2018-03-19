#ifndef __EQUIVALENCE_H__
#define __EQUIVALENCE_H__

#pragma once


#include "cpulist.h"

#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>


class EqualCPUS
{
   private:
    CPUList         _cpulist;
    std::vector<int>    _cpus;


    friend class Equivalence;

    std::map<int, int> conversion_map(const EqualCPUS& other) const
    {
        if (other._cpulist == _cpulist)
            return {};

        if (_cpus.size() != other._cpus.size())
            throw std::runtime_error{"Can't generate conversion map for mapping of different equivalence classes."};

        std::map<int, int> result;

        for (size_t i = 0; i < _cpus.size(); ++i) {
            if (_cpus[i] != other._cpus[i])
                result.emplace(_cpus[i], other._cpus[i]);
        }

        return result;
    }

   public:
    EqualCPUS(const std::initializer_list<int>& cpus) :
        _cpulist{cpus}, _cpus{cpus}
    {}

    bool operator==(const CPUList& o) const
    {
        return _cpulist == o;
    }
};

bool operator==(const CPUList& c, const EqualCPUS& ec);


class Equivalence
{
   private:
    std::string     _name;
    std::vector<EqualCPUS>  _equalcpus;

   public:
    Equivalence(const std::string& name, const std::initializer_list<EqualCPUS>& equalcpulists) :
        _name{name}, _equalcpus{equalcpulists}
    {}

    const std::string name() const
    {
        return _name;
    }

    bool is_in_equalence_class(const CPUList& cpulist) const
    {
        for (const auto& ecpus : _equalcpus) {
            if (cpulist == ecpus)
                return true;
        }

        return false;
    }

    std::vector<std::map<int, int>> equivalent_mappings(const CPUList& cpulist) const
    {
        for (const auto& ecpus : _equalcpus) {
            if (cpulist == ecpus) {
                std::vector<std::map<int, int>> result;

                for (const auto& other : _equalcpus)
                    result.push_back(ecpus.conversion_map(other));

                return result;
            }
        }

        throw std::runtime_error{"This mapping is not part of this equivalence class."};
    }
};

#endif /* __EQUIVALENCE_H__ */
