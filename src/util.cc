// See LICENSE for license details.

#include <cstdlib>
#include <cstring>
#include <cerrno>

#ifndef _WIN32
#include <dirent.h>
#include <sys/types.h>
#endif

#include <vector>
#include <string>
#include <algorithm>

#include "util.h"
#include "logger.h"

#ifdef _WIN32
std::vector<std::string> listFiles(std::string dirname)
{
    WIN32_FIND_DATA ffd;
    HANDLE hFind;
    char path[PATH_MAX];
    std::vector<std::string> list;

    std::string findname = dirname + "/*";
    hFind = FindFirstFileA(findname.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        Error("%s: FindFirstFileA error: %s: %d\n", __func__,
            dirname.c_str(), GetLastError());
        goto out;
    }

    do {
        snprintf(path, sizeof(path), "%s/%s",
                dirname.size() == 0 ? "." : dirname.c_str(), ffd.cFileName);
        list.push_back(path);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
out:
    return list;
}
#else
std::vector<std::string> listFiles(std::string dirname)
{
    DIR *dirp;
    struct dirent *r;
    char path[PATH_MAX];
    std::vector<std::string> list;

    if (!(dirp = opendir(dirname.c_str()))) {
        Error("%s: opendir error: %s: %s", __func__,
            dirname.c_str(), strerror(errno));
        goto out;
    }

    while ((r = readdir(dirp))) {
        if (strcmp(r->d_name, ".") == 0 || strcmp(r->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s",
            dirname.size() == 0 ? "." : dirname.c_str(), r->d_name);
        list.push_back(path);
    }

    closedir(dirp);

out:
    return list;
}
#endif

std::vector<std::string> sortList(std::vector<std::string> l)
{
    std::sort(l.begin(), l.end());
    return l;
}

std::vector<std::string> endsWith(std::vector<std::string> l, std::string ext)
{
    std::vector<std::string> list;
    for (auto &p : l) {
        size_t i = p.find(ext);
        if (i == p.size() - ext.size()) {
            list.push_back(p);
        }
    }
    return list;
}
