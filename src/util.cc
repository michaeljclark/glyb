#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <dirent.h>

#include <vector>
#include <string>
#include <algorithm>

#include "util.h"

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

std::vector<std::string> listFiles(std::string dirname)
{
	DIR *dirp;
	struct dirent *r;
	char path[PATH_MAX];
	std::vector<std::string> list;

	if (!(dirp = opendir(dirname.c_str()))) {
		fprintf(stderr, "%s: opendir error: %s: %s",
			__func__, dirname.c_str(), strerror(errno));
		goto out;
	}

	while ((r = readdir(dirp))) {
		if (strcmp(r->d_name, ".") == 0 || strcmp(r->d_name, "..") == 0) {
			continue;
		}
		snprintf(path, sizeof(path), "%s" PATH_SEPARATOR "%s",
			dirname.size() == 0 ? "." : dirname.c_str(), r->d_name);
		list.push_back(path);
	}

	closedir(dirp);

out:
	return list;
}

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
