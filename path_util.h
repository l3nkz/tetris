#ifndef __PATH_UTIL_H__
#define __PATH_UTIL_H__

#pragma once


#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include <dirent.h>
#include <pwd.h>
#include <unistd.h>


namespace path_util {

/* Prototypes */
std::string abspath(const std::string&);
std::string basename(const std::string&);
std::string dirname(const std::string&);
bool exists(const std::string&);
std::string extension(const std::string&);
std::string expanduser(const std::string&);
std::string filename(const std::string&);
void for_each_file(const std::string&, std::function<void(const std::string&)>&);
std::string getcwd();
bool isabs(const std::string&);
std::string join(const std::string&, const std::string&, char delim='/');
std::pair<std::string, std::string> split(const std::string&, char delim='/');
std::pair<std::string, std::string> splitext(const std::string&, char delim='.');


/* Implementations */
std::string abspath(const std::string& path)
{
    return isabs(path) ? path : join(getcwd(), path);
}

std::string basename(const std::string& path)
{
    return split(path).second;
}

std::string dirname(const std::string& path)
{
    return split(path).first;
}

bool exists(const std::string& path)
{
    return ::access(path.c_str(), F_OK) == 0;
}

std::string expanduser(const std::string& path)
{
    if (path.empty()) {
        return path;
    } else {
        if (path[0] == '~') {
            return std::string{getpwuid(getuid())->pw_dir} + path.substr(1);
        } else {
            return path;
        }
    }
}

std::string extension(const std::string& path)
{
    return splitext(path).second;
}

std::string filename(const std::string& path)
{
    return splitext(split(path).second).first;
}

void for_each_file(const std::string& path, std::function<void(const std::string&)> cb)
{
    auto dir = opendir(path.c_str());
    if (dir == nullptr) {
        throw std::runtime_error{"Failed to open directory at " + path};
    }

    dirent* cur;
    while ((cur =readdir(dir)) != nullptr) {
        if (cur->d_type == DT_REG || cur->d_type == DT_LNK || cur->d_type == DT_UNKNOWN) {
            std::string file_name{cur->d_name};

            cb(join(path, file_name));
        }
    }
}

std::string getcwd()
{
    char cwd[512];
    ::getcwd(cwd, sizeof(cwd));

    return std::string{cwd};
}

bool isabs(const std::string& path)
{
    if (path.empty())
        return false;
    return path[0] == '/';
}

std::string join(const std::string& first, const std::string& second, char delim)
{
    return first + delim + second;
}

std::pair<std::string, std::string> split(const std::string& path, char delim)
{
    size_t dpos = std::string::npos;
    while (dpos != 0) {
        dpos = path.rfind(delim, dpos);

        if (dpos == std::string::npos) {
            return std::make_pair("", path);
        } else if (dpos != 0) {
            if (path[dpos-1] == '\\')
                continue;

            return std::make_pair(path.substr(0, dpos), path.substr(dpos+1));
        } else {
            return std::make_pair("/", path.substr(1));
        }
    }

    return std::make_pair(path, "");
}

std::pair<std::string, std::string> splitext(const std::string& path, char delim)
{
    size_t dpos = std::string::npos;
    while (dpos != 0) {
        dpos = path.rfind(delim, dpos);

        if (dpos == std::string::npos) {
            return std::make_pair(path, "");
        } else if (dpos != 0) {
            if (path[dpos-1] == '\\')
                continue;

            return std::make_pair(path.substr(0, dpos), path.substr(dpos));
        } else if (dpos == 0) {
            return std::make_pair(path, "");
        }
    }

    return std::make_pair(path, "");
}

} /* namespace path_util */

#endif /* __PATH_UTIL_H__ */
