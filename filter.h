#ifndef __FILTER_H__
#define __FILTER_H__

#pragma once


#include "string_util.h"
#include "mapping.h"

#include <functional>
#include <iostream>
#include <string>


namespace detail {

struct FilterComp
{
    virtual ~FilterComp() = default;

    virtual bool comp(const double lhs, const double rhs) = 0;
    virtual FilterComp* clone() = 0;
};

template <typename BaseComp>
struct StdComp : public FilterComp
{
    bool comp(const double lhs, const double rhs)
    {
        return BaseComp{}(lhs, rhs);
    }

    FilterComp* clone()
    {
        return new StdComp<BaseComp>;
    }
};

struct NoComp : public FilterComp
{
    bool comp(const double, const double)
    {
        return true;
    }

    FilterComp* clone()
    {
        return new NoComp;
    }
};

} /* anonymous namespace */


class Filter
{
   private:
    std::string     _criteria;
    double          _value;

    detail::FilterComp*     _comp;

    void set_comperator(detail::FilterComp* comp)
    {
        if (comp == _comp)
            return;

        if (_comp)
            delete _comp;

        _comp = comp;
    }

   public:
    Filter(const std::string& filter_criteria) :
        _comp{nullptr}
    {
        /* We currently support 6 comparisons:
         *      >=
         *      >
         *      <=
         *      <
         *      == or =
         *      !=
         *
         * Search for one of them in the given string */
        bool valid = false;
        std::string criteria, value;

        for (size_t i = 1; i < filter_criteria.size(); ++i) {
            switch (filter_criteria[i-1]) {
                case '>':
                    criteria = filter_criteria.substr(0, i-1);

                    if (filter_criteria[i] == '=') {
                        set_comperator(new detail::StdComp<std::greater_equal<double>>);
                        value = filter_criteria.substr(i+1);
                    } else {
                        set_comperator(new detail::StdComp<std::greater<double>>);
                        value = filter_criteria.substr(i);
                    }

                    valid = true;
                    break;
                case '<':
                    criteria = filter_criteria.substr(0, i-1);

                    if (filter_criteria[i] == '=') {
                        set_comperator(new detail::StdComp<std::less_equal<double>>);
                        value = filter_criteria.substr(i+1);
                    } else {
                        set_comperator(new detail::StdComp<std::less<double>>);
                        value = filter_criteria.substr(i);
                    }

                    valid = true;
                    break;
                case '=':
                    criteria = filter_criteria.substr(0, i-1);
                    set_comperator(new detail::StdComp<std::equal_to<double>>);

                    if (filter_criteria[i] == '=') {
                        value = filter_criteria.substr(i+1);
                    } else {
                        value = filter_criteria.substr(i);
                    }

                    valid = true;
                    break;
                case '!':
                    if (filter_criteria[i] == '=') {
                        value = filter_criteria.substr(i+1);
                        criteria = filter_criteria.substr(0, i-1);
                        set_comperator(new detail::StdComp<std::not_equal_to<double>>);

                        valid = true;
                    }

                    break;
            }

            if (valid)
                break;
        }

        if (valid) {
            _criteria = string_util::strip(criteria);
            _value = std::stod(string_util::strip(value));
        } else {
            std::cerr << "Failed to parse filter string -- use no filter" << std::endl;
            set_comperator(new detail::NoComp);
        }
    }

    Filter() :
        _comp{new detail::NoComp}
    {}

    Filter(const Filter& o) :
        _criteria{o._criteria}, _value{o._value}, _comp{o._comp->clone()}
    {}

    Filter(Filter&& o) :
        _criteria{std::move(o._criteria)}, _value{std::move(o._value)}, _comp{o._comp}
    {
        o._comp = nullptr;
    }

    ~Filter()
    {
        if (_comp)
            delete _comp;
    }

    Filter& operator=(const Filter& o)
    {
        _criteria = o._criteria;
        _value = o._value;
        set_comperator(o._comp->clone());

        return *this;
    }

    Filter& operator=(Filter&& o)
    {
        _criteria = std::move(o._criteria);
        _value = std::move(o._value);
        set_comperator(o._comp);

        o._comp = nullptr;

        return *this;
    }

    bool operator()(const Mapping& map)
    try {
        return _comp->comp(_value, map.characteristic(_criteria));
    } catch (std::runtime_error&) {
        return false;
    }
};

#endif /* __FILTER_H__ */
