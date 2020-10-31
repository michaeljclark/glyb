#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cstdlib>
#include <climits>
#include <cctype>

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <atomic>
#include <mutex>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include "binpack.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "file.h"

static font_manager_ft manager;

static const char* font_dir = "fonts";
static bool print_font_list = false;
static bool print_block_stats = false;
static bool help_text = false;
static float cover_min = 0.05;
static int family_width = 80;

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -d, --font-dir <name>        font dir\n"
        "  -l, --list                   list fonts\n"
        "  -l, --stats                  show font stats (block)\n"
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
        if (match_opt(argv[i], "-d", "--font-dir")) {
            if (check_param(++i == argc, "--font-dir")) break;
            font_dir = argv[i++];
        } else if (match_opt(argv[i], "-c", "--cover-min")) {
            if (check_param(++i == argc, "--cover-min")) break;
            cover_min = atof(argv[i++]);
        } else if (match_opt(argv[i], "-w", "--family-width")) {
            if (check_param(++i == argc, "--family-width")) break;
            family_width = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-b", "--block-stats")) {
            print_block_stats = true;
            i++;
        } else if (match_opt(argv[i], "-l", "--list")) {
            print_font_list = true;
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

static bool endsWith(std::string str, std::string ext)
{
    size_t i = str.find(ext);
    return (i == str.size() - ext.size());
}

void scanFontDir(std::string dir)
{
    std::vector<std::string> dirs;
    std::vector<std::string> fontfiles;
    size_t i = 0;
    dirs.push_back(dir);
    while(i < dirs.size()) {
        std::string current_dir = dirs[i++];
        for (auto &name : file::list(current_dir)) {
            if (file::dirExists(name)) {
                dirs.push_back(name);
            } else if (endsWith(name, ".ttf")) {
                fontfiles.push_back(name);
            }
        }
    }
    for (auto &name : fontfiles) {
        manager.scanFontPath(name);
    }
}

static std::vector<uint> allCodepoints(FT_Face ftface)
{
    std::vector<uint> l;
    unsigned glyph, codepoint = FT_Get_First_Char(ftface, &glyph);
    do {
        l.push_back(codepoint);
        codepoint = FT_Get_Next_Char(ftface, codepoint, &glyph);
    } while (glyph);
    return l;
}

void do_print_font_list()
{
    for (auto &font : manager.getFontList()) {
        printf("%s\n", font->getFontData().toString().c_str());
    }
}

static std::string ltrim(std::string s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

static std::string rtrim(std::string s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

struct block {
    uint start;
    uint end;
    std::string name;
};

std::vector<block> read_blocks()
{
    // # @missing: 0000..10FFFF; No_Block
    // 0000..007F; Basic Latin
    std::vector<block> blocks;
    FILE *f;
    char buf[128];
    const char* p;

    if ((f = fopen("data/unicode/Blocks.txt", "r")) == nullptr) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        exit(1);
    }
    while((p = fgets(buf, sizeof(buf), f)) != nullptr) {
        auto l = ltrim(rtrim(std::string(p)));
        if (l.size() == 0) continue;
        if (l.find("#") != std::string::npos) continue;
        size_t d = l.find("..");
        size_t s = l.find(";");
        blocks.push_back({
            (uint)strtoul(l.substr(0,d).c_str(),nullptr, 16),
            (uint)strtoul(l.substr(d+2,s-d-2).c_str(),nullptr, 16),
            l.substr(s+2)
        });
    }
    fclose(f);

    return blocks;
}

uint find_block(std::vector<block> &blocks, uint32_t cp)
{
    uint i = 0;
    for (auto &b: blocks) {
        if (cp >= b.start && cp <= b.end) return i;
        i++;
    }
    return blocks.size()-1;
}

template <typename K, typename V>
void hist_add(std::map<K,V> &hist, K key, V val)
{
    auto hci = hist.find(key);
    if (hci == hist.end()) {
        hist.insert(hist.end(),std::pair<K,V>(key,val));
    } else {
        hci->second += val;
    }
}

template <typename K>
struct id_map
{
    uint id;
    std::map<K,uint> map;
    std::map<uint,K> rmap;

    id_map() : id(0), map() {}

    uint get_id(K key) {
        auto i = map.find(key);
        if (i == map.end()) {
            i = map.insert(map.end(), std::pair<K,uint>(key, id++));
            rmap[i->second] = key;
        }
        return i->second;
    }

    K get_key(uint i) { return rmap[i]; }
};

template <typename T, typename K, typename V>
auto find_or_insert(T &map, K key, V def)
{
    auto i = map.find(key);
    if (i == map.end()) {
        i = map.insert(map.end(), std::pair<K,V>(key, def));
    }
    return i;
}

std::string truncate(std::string str, size_t sz)
{
    if (str.length() < sz) return str;
    else return str.substr(0, sz) + "...";
}

struct block_family_data
{
    std::map<font_face*,uint> codes;
};

struct block_data
{
    std::map<uint,block_family_data> families;
};

struct family_data
{
    std::string family_name;
    std::string font_names;
    uint family_count;
    uint glyph_count;
};

template <typename K, typename V, typename F>
std::string to_string(std::map<K,V> &list, F fn, std::string sep = ", ")
{
    std::string str;
    auto i = list.begin();
    if (i == list.end()) goto out;
    str.append(fn(i));
    if (++i == list.end()) goto out;
    for (; i != list.end(); i++) {
        str.append(sep);
        str.append(fn(i));
    }
    out: return str;
}

std::string remove_prefix(std::string &str, std::string sep)
{
    auto i = str.find(sep);
    return (i != std::string::npos) ? str.substr(i+1) : str;
}

void do_print_block_stats()
{
    id_map<std::string> font_name_map;
    id_map<std::string> font_family_map;
    std::vector<block> blocks;
    std::map<uint,block_data> block_stats;

    blocks = read_blocks();
    blocks.push_back({0,0xfffff,"Unknown"});
    for (auto &font : manager.getFontList()) {
        FT_Face ftface = static_cast<font_face_ft*>(font.get())->ftface;
        uint font_name_id = font_name_map.get_id(font->path);
        uint font_family_id = font_family_map.get_id(font->fontData.familyName);
        FT_Select_Charmap(ftface, FT_ENCODING_UNICODE);
        auto cplist = allCodepoints(ftface);
        for (auto cp : cplist) {
            uint bc = find_block(blocks,cp);
            auto bsi = find_or_insert(block_stats, bc, block_data());
            auto fsi = find_or_insert(bsi->second.families, font_family_id, block_family_data());
            auto gsi = find_or_insert(fsi->second.codes, font.get(), 0);
            gsi->second++;
        }
    }
    for (size_t i = 0; i < blocks.size(); i++) {
        auto &b = blocks[i];
        auto bsi = block_stats.find(i);
        if (bsi->second.families.size() == 0) continue;
        printf("%06x..%06x; %-80s\n", b.start, b.end, b.name.c_str());
        std::vector<family_data> fam_data;
        for (auto &ent : bsi->second.families) {
            uint font_family_id = ent.first;
            block_family_data &block_family_data = ent.second;
            std::string family_name = font_family_map.get_key(font_family_id);
            std::string font_names = to_string(block_family_data.codes, [](auto i) {
                return remove_prefix(i->first->name, "-");
            });
            uint glyph_count = 0;
            for (auto ent : block_family_data.codes) glyph_count += ent.second;
            uint family_count = (uint)block_family_data.codes.size();
            fam_data.push_back(family_data{family_name, font_names, family_count, glyph_count});
        }
        std::sort(fam_data.begin(), fam_data.end(), [](auto a, auto b) {
            return a.glyph_count/a.family_count > b.glyph_count/b.family_count;
        });
        for (auto &ent: fam_data) {
            uint glyph_avg = ent.glyph_count/ent.family_count;
            uint block_glyphs = (b.end - b.start);
            float cover = (float)glyph_avg/(float)block_glyphs;
            if (cover < cover_min) continue;
            printf("\t%5.2f %10u,%-10u %-20s %s\n",
                cover,
                ent.family_count,
                ent.glyph_count/ent.family_count,
                ent.family_name.c_str(),
                truncate(ent.font_names, family_width).c_str());
        }
    }
}

int main(int argc, char **argv)
{
    parse_options(argc, argv);

    scanFontDir(font_dir);

    if (print_font_list) do_print_font_list();
    if (print_block_stats) do_print_block_stats();
}
