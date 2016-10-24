#ifndef __CPULIST_H__
#define __CPULIST_H__

#pragma once


#include <algorithm>
#include <initializer_list>
#include <map>
#include <vector>


class CPUList
{
   private:
    std::vector<int>    _cpus;

   public:
    CPUList() :
        _cpus{}
    {}

    CPUList(const std::vector<int>& cpus) :
        _cpus{cpus}
    {}

    CPUList(const std::initializer_list<int>& cpus) :
        _cpus{cpus}
    {}

    bool operator==(const CPUList& o) const
    {
        if (_cpus.size() != o._cpus.size()) {
            return false;
        } else {
            for (auto i1 = _cpus.begin(), i2 = o._cpus.begin(); i1 != _cpus.end(); ++i1, ++i2) {
                if (*i1 != *i2)
                    return false;
            }
        }

        return true;
    }

    bool operator!=(const CPUList& o) const
    {
        return !(*this == o);
    }

    void set(int cpu_nr)
    {
        if (std::find(_cpus.begin(), _cpus.end(), cpu_nr) != _cpus.end())
            _cpus.push_back(cpu_nr);
    }

    void clear(int cpu_nr)
    {
        auto i = std::find(_cpus.begin(), _cpus.end(), cpu_nr);

        if (i != _cpus.end())
            _cpus.erase(i);
    }

    bool overlaps_with(const CPUList& o) const
    {
        for (auto c : _cpus) {
            if (std::find(o._cpus.begin(), o._cpus.end(), c) != o._cpus.end())
                return true;
        }

        return false;
    }

    std::map<int,int> convert_map(const CPUList& to) const
    {
        std::map<int, int> res;

        for (auto i1 = _cpus.begin(), i2 = to._cpus.begin(); i1 != _cpus.end(); ++i1, ++i2) {
            if (i2 != to._cpus.end())
                res.emplace(*i1, *i2);
            else
                res.emplace(*i1, 0);
        }

        return res;

    }
};

#endif /* __CPULIST_H__ */
