#ifndef __CPULIST_H__
#define __CPULIST_H__

#pragma once


#include <algorithm>
#include <initializer_list>
#include <map>
#include <vector>

#include <sched.h>


class CPUList
{
   private:
    cpu_set_t    _cpus;

   public:
    CPUList()
    {
        CPU_ZERO(&_cpus);
    }

    template <template<typename> class Container>
    CPUList(const Container<int>& cpus) :
        CPUList{}
    {
        for (const auto c : cpus)
            CPU_SET(c, &_cpus);
    }

    CPUList(const std::initializer_list<int>& cpus) :
        CPUList{}
    {
        for (const auto c : cpus)
            CPU_SET(c, &_cpus);
    }

    explicit CPUList(cpu_set_t cpus) :
        _cpus{cpus}
    {}

    CPUList(const CPUList& o) = default;
    CPUList(CPUList&& o) = default;

    CPUList& operator=(const CPUList& o) = default;
    CPUList& operator=(CPUList&& o) = default;

    CPUList& operator=(cpu_set_t cpus)
    {
        _cpus = cpus;

        return *this;
    }

    bool operator==(const CPUList& o) const
    {
        return CPU_EQUAL(&_cpus, &o._cpus);
    }

    bool operator!=(const CPUList& o) const
    {
        return !(*this == o);
    }

    CPUList operator&(const CPUList& o) const
    {
        CPUList tmp{};

        CPU_AND(&tmp._cpus, &_cpus, &o._cpus);

        return tmp;
    }

    CPUList& operator&=(const CPUList& o)
    {
        CPU_AND(&_cpus, &_cpus, &o._cpus);

        return *this;
    }

    CPUList operator|(const CPUList& o) const
    {
        CPUList tmp{};

        CPU_OR(&tmp._cpus, &_cpus, &o._cpus);

        return tmp;
    }

    CPUList operator|=(const CPUList& o)
    {
        CPU_OR(&_cpus, &_cpus, &o._cpus);

        return *this;
    }

    void set(int cpu_nr)
    {
        CPU_SET(cpu_nr, &_cpus);
    }

    void clear(int cpu_nr)
    {
        CPU_CLR(cpu_nr, &_cpus);
    }

    void zero()
    {
        CPU_ZERO(&_cpus);
    }

    bool overlaps_with(const CPUList& o) const
    {
        cpu_set_t tmp;

        CPU_AND(&tmp, &_cpus, &o._cpus);

        return CPU_COUNT(&tmp) != 0;
    }

    int nr_cpus() const
    {
        return CPU_COUNT(&_cpus);
    }

    cpu_set_t cpu_set() const
    {
        return _cpus;
    }

    std::vector<int> cpulist(int max_cpus) const
    {
        std::vector<int> result;

        for (int i = 0; i < max_cpus; ++i)
            if (CPU_ISSET(i, &_cpus))
                result.push_back(i);

        return result;
    }
};

#endif /* __CPULIST_H__ */
