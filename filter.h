#ifndef __FILTER_H__
#define __FILTER_H__

#pragma once


#include "debug_util.h"
#include "string_util.h"
#include "mapping.h"

#include <functional>
#include <sstream>
#include <string>


namespace detail {

class FilterComp
{
   public:
    virtual ~FilterComp() = default;

    virtual bool comp(const Mapping& map) const = 0;
    virtual FilterComp* clone() const = 0;

    virtual std::string criteria() const = 0;
    virtual std::string repr() const = 0;
};

template <typename BaseComp>
class StdComp : public FilterComp
{
   private:
    std::string     _criteria;
    double          _value;

    BaseComp        _comp;

   public:
    StdComp(const std::string& criteria, const double value) :
        _criteria{criteria}, _value{value}, _comp{}
    {}

    bool comp(const Mapping& map) const
    {
        return _comp(map.characteristic(_criteria), _value);
    }

    FilterComp* clone() const
    {
        return new StdComp<BaseComp>(_criteria, _value);
    }

    std::string criteria() const
    {
        return _criteria;
    }

    std::string repr() const
    {
        std::stringstream ss;

        ss << _criteria << debug::CompRepr<BaseComp>::repr << _value;
        return ss.str();
    }
};

struct NoComp : public FilterComp
{
    bool comp(const Mapping&) const
    {
        return true;
    }

    FilterComp* clone() const
    {
        return new NoComp;
    }

    std::string criteria() const
    {
        return "none";
    }

    std::string repr() const
    {
        return "none";
    }
};

enum Type : int {
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    EQUAL,
    NOT_EQUAL,
    NONE,
    ERROR
};

} /* namespace detail*/


class Filter
{
   private:
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
        detail::Type type = detail::Type::ERROR;
        std::string criteria, value;

        for (size_t i = 1; i < filter_criteria.size(); ++i) {
            switch (filter_criteria[i-1]) {
                case '>':
                    criteria = filter_criteria.substr(0, i-1);

                    if (filter_criteria[i] == '=') {
                        type = detail::Type::GREATER_EQUAL;
                        value = filter_criteria.substr(i+1);
                    } else {
                        type = detail::Type::GREATER;
                        value = filter_criteria.substr(i);
                    }

                    break;
                case '<':
                    criteria = filter_criteria.substr(0, i-1);

                    if (filter_criteria[i] == '=') {
                        type = detail::Type::LESS_EQUAL;
                        value = filter_criteria.substr(i+1);
                    } else {
                        type = detail::Type::LESS;
                        value = filter_criteria.substr(i);
                    }

                    break;
                case '=':
                    criteria = filter_criteria.substr(0, i-1);
                    type = detail::Type::EQUAL;

                    if (filter_criteria[i] == '=') {
                        value = filter_criteria.substr(i+1);
                    } else {
                        value = filter_criteria.substr(i);
                    }

                    break;
                case '!':
                    if (filter_criteria[i] == '=') {
                        type = detail::Type::NOT_EQUAL;
                        value = filter_criteria.substr(i+1);
                        criteria = filter_criteria.substr(0, i-1);
                    }

                    break;
            }

            if (type != detail::Type::ERROR)
                break;
        }

        switch (type) {
            case detail::Type::GREATER:
                set_comperator(new detail::StdComp<std::greater<double>>(string_util::strip(criteria),
                            std::stod(string_util::strip(value))));
                break;
            case detail::Type::GREATER_EQUAL:
                set_comperator(new detail::StdComp<std::greater_equal<double>>(string_util::strip(criteria),
                            std::stod(string_util::strip(value))));
                break;
            case detail::Type::LESS:
                set_comperator(new detail::StdComp<std::less<double>>(string_util::strip(criteria),
                            std::stod(string_util::strip(value))));
                break;
            case detail::Type::LESS_EQUAL:
                set_comperator(new detail::StdComp<std::less_equal<double>>(string_util::strip(criteria),
                            std::stod(string_util::strip(value))));
                break;
            case detail::Type::EQUAL:
                set_comperator(new detail::StdComp<std::equal_to<double>>(string_util::strip(criteria),
                            std::stod(string_util::strip(value))));
                break;
            case detail::Type::NOT_EQUAL:
                set_comperator(new detail::StdComp<std::not_equal_to<double>>(string_util::strip(criteria),
                            std::stod(string_util::strip(value))));
                break;
            default:
                set_comperator(new detail::NoComp);
        }
    }

    Filter() :
        _comp{new detail::NoComp}
    {}

    Filter(const Filter& o) :
        _comp{o._comp->clone()}
    {}

    Filter(Filter&& o) :
        _comp{o._comp}
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
        set_comperator(o._comp->clone());

        return *this;
    }

    Filter& operator=(Filter&& o)
    {
        set_comperator(o._comp);

        o._comp = nullptr;

        return *this;
    }

    std::string criteria() const
    {
        return _comp->criteria();
    }

    std::string repr() const
    {
        return _comp->repr();
    }

    bool operator()(const Mapping& map) const
    try {
        return _comp->comp(map);
    } catch (std::runtime_error&) {
        return false;
    }
};

#endif /* __FILTER_H__ */
