// See LICENSE for license details.

#pragma once

#ifdef _WIN32
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#endif

std::vector<std::string> listFiles(std::string dirname);
std::vector<std::string> sortList(std::vector<std::string> l);
std::vector<std::string> endsWith(std::vector<std::string> l, std::string ext);
