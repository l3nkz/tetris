#ifndef __STRING_UTIL_H__
#define __STRING_UTIL_H__

#pragma once


#include <cctype>
#include <sstream>
#include <string>
#include <vector>


namespace string_util {

/* Prototypes */
bool ends_with(const std::string&, const std::string&);
template <template<typename> class Container, typename T>
std::string join(const Container<T>&, const std::string&);
template <template<typename> class Container, typename T>
std::string join(const Container<T>&, const char delim=' ');
std::string lstrip(const std::string&);
std::string rstrip(const std::string&);
std::vector<std::string> split(const std::string&, char delim=' ');
std::vector<std::string> split(const std::string&, const std::string&);
bool starts_with(const std::string&, const std::string&);
std::string strip(const std::string&);


/* Implementations */
bool ends_with(const std::string& s, const std::string& end)
{
    for (auto i1 = s.rbegin(), i2 = end.rbegin(); i2 != end.rend(); ++i1, ++i2) {
        if (i1 == s.rend())
            return false;
        else if (*i1 != *i2)
            return false;
    }

    return true;
}

template <template<typename> class Container, typename T>
std::string join(const Container<T>& subs, const char delim)
{
    return join(subs, std::string{delim});
}

template <template<typename> class Container>
std::string join(const Container<std::string>& subs, const std::string& delim)
{
    if (subs.size() == 0) {
        return {};
    } else {
        std::string result;
        for (auto& sub : subs) {
            result += sub + delim;
        }

        /* Remove the last 'delim' again. */
        result.erase(result.size() - delim.size());

        return result;
    }
}

template <template<typename> class Container, typename T>
std::string join(const Container<T>& subs, const std::string& delim)
{
    std::vector<std::string> string_subs;

    for (const auto& s : subs) {
        std::stringstream ss;
        ss << s;

        string_subs.push_back(ss.str());
    }

    return join(string_subs, delim);
}

std::string lstrip(const std::string& s)
{
    std::string result;
    auto i = s.begin();

    while (i != s.end()) {
        if (std::isspace(*i) != 0 || std::iscntrl(*i) != 0)
            ++i;
        else
            break;
    }

    while (i != s.end()) {
        result.push_back(*i);
        ++i;
    }

    return result;
}

std::string rstrip(const std::string& s)
{
    std::string result;
    auto i = s.rbegin();

    while (i != s.rend()) {
        if (std::isspace(*i) != 0 || std::iscntrl(*i) != 0)
            ++i;
        else
            break;
    }

    while (i != s.rend()) {
        result = *i + result;
        ++i;
    }

    return result;
}

std::vector<std::string> split(const std::string& s, const char delim)
{
    return split(s, std::string{delim});
}

std::vector<std::string> split(const std::string& s, const std::string& delim)
{
    std::vector<std::string> subs;
    size_t dpos = 0, oldpos = 0;

    while (dpos != std::string::npos) {
        dpos = s.find(delim, oldpos);

        if (dpos == std::string::npos)
            subs.push_back(s.substr(oldpos));
        else
            subs.push_back(s.substr(oldpos, dpos-oldpos));

        oldpos = dpos + delim.size();
    }

    return subs;
}

bool starts_with(const std::string& s, const std::string& start)
{
    for (auto i1 = s.begin(), i2 = start.begin(); i2 != start.end(); ++i1, ++i2) {
        if (i1 == s.end())
            return false;
        else if (*i1 != *i2)
            return false;
    }

    return true;
}

std::string strip(const std::string& s)
{
    return rstrip(lstrip(s));
}

} /* namespace string_util */

#endif /* __STRING_UTIL_H__ */
