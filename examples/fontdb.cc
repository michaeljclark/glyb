#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <climits>

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <tuple>
#include <chrono>
#include <algorithm>

#include <sys/types.h>
#include <dirent.h>

#include "binpack.h"
#include "font.h"
#include "glyph.h"

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

using namespace std::chrono;

static font_manager_ft manager;
static font_atlas atlas;

static const char* font_dir = "fonts";
static bool print_list = false;
static bool help_text = false;
static int font_weight = -1;
static int font_slope = -1;
static int font_stretch = -1;
static const char* font_name = "*";

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -n, --font-name <name>       font name\n"
        "  -w, --font-weight <weight>   font weight\n"
        "  -s, --font-slant <slant>     font slant\n"
        "  -S, --font-stretch <stretch> font stretch\n"
        "  -l, --list                   list fonts\n"
        "  -h, --help                   command line help\n",
        argv[0]);
}


bool check_param(bool cond, const char *param)
{
    if (cond) {
        printf("error: %s requires parameter\n", param);
    }
    return (help_text = cond);
}

bool match_opt(const char *arg, const char *opt, const char *longopt)
{
    return strcmp(arg, opt) == 0 || strcmp(arg, longopt) == 0;
}

void parse_options(int argc, char **argv)
{
    int i = 1;
    while (i < argc) {
        if (match_opt(argv[i], "-d","--font-dir")) {
            if (check_param(++i == argc, "--font-dir")) break;
            font_dir = argv[i++];
        }
        else if (match_opt(argv[i], "-n","--font-name")) {
            if (check_param(++i == argc, "--font-name")) break;
            font_name = argv[i++];
        }
        else if (match_opt(argv[i], "-w", "--font-weight")) {
            if (check_param(++i == argc, "--font-weight")) break;
            font_weight = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-s", "--font-slant")) {
            if (check_param(++i == argc, "--font-slant")) break;
            font_slope = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-S", "--font-stretch")) {
            if (check_param(++i == argc, "--font-stretch")) break;
            font_stretch = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-l", "--list")) {
            print_list = true;
            i++;
        } else if (match_opt(argv[i], "-h", "--help")) {
            help_text = true;
            i++;
        } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            help_text = true;
            break;
        }
    }

    if (help_text) {
        print_help(argc, argv);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    parse_options(argc, argv);

    manager.scanFontDir(font_dir);

    if (print_list) {
        for (auto &font : manager.getFontList()) {
            printf("%s\n", font.getFontData().toString().c_str());
        }
    }
}
