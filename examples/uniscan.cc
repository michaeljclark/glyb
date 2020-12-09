#undef NDEBUG
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cctype>
#include <cinttypes>

#include <map>
#include <set>
#include <chrono>
#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_set>

#include "utf8.h"
#include "bitcode.h"
#include "format.h"
#include "hashmap.h"

using namespace std::chrono;

using string = std::string;
template <typename T> using vector = std::vector<T>;
template <typename T> using set = std::set<T>;
template <typename K,typename V> using pair = std::pair<K,V>;
template <typename K,typename V> using map = std::map<K,V>;
template <typename T, typename H> using hash_set = std::unordered_set<T,H>;
typedef unsigned int uint;

static const char* blocks_file = "data/unicode/Blocks.txt";
static const char* data_file = "data/unicode/UnicodeData.txt";
static const char* search_data;
static bool optimized_search = true;
static bool print_data = false;
static bool print_blocks = false;
static bool debug_symbols = false;
static bool debug_tree = false;
static bool debug_flat = false;
static bool debug_comments = false;
static bool compress_stats = false;
static bool experiment = false;
static bool compress_data = false;
static bool help_text = false;

static vector<string> split(string str,
    string sep, bool inc_sep, bool inc_empty)
{
    size_t i, j = 0;
    vector<string> comps;
    while ((i = str.find_first_of(sep, j)) != string::npos) {
        if (inc_empty || i - j > 0) comps.push_back(str.substr(j, i - j));
        if (inc_sep) comps.push_back(str.substr(i, sep.size()));
        j = i + sep.size();
    }
    if (inc_empty || str.size() - j > 0) {
        comps.push_back(str.substr(j, str.size() - j));
    }
    return comps;
}

static vector<string> split(string str,
    string exc_sep, string inc_sep)
{
    size_t h, i, j = 0;
    vector<string> comps;
    while (true) {
        h = str.find_first_of(exc_sep, j);
        i = str.find_first_of(inc_sep, j);
        if (h == string::npos && i == string::npos) break;
        if (h != string::npos && (i == string::npos || h < i)) {
            if (h - j > 0) comps.push_back(str.substr(j, h - j));
            j = h + 1; /* assumes separator is 1-byte */
        } else {
            comps.push_back(str.substr(j, i - j + 1));
            j = i + 1; /* assumes separator is 1-byte */
        }
    }
    if (str.size() - j > 0) {
        comps.push_back(str.substr(j, str.size() - j));
    }
    return comps;
}

static string join(vector<string> comps,
    string sep, size_t start, size_t end)
{
    string str;
    for (size_t i = start; i != end; i++) {
        if (i != start) {
            str.append(sep);
        }
        str.append(comps[i]);
    }
    return str;
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
    string name;
};

vector<block> read_blocks()
{
    // # @missing: 0000..10FFFF; No_Block
    // 0000..007F; Basic Latin
    vector<block> blocks;
    FILE *f;
    char buf[128];
    const char* p;

    if ((f = fopen(blocks_file, "r")) == nullptr) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        exit(1);
    }
    while((p = fgets(buf, sizeof(buf), f)) != nullptr) {
        auto l = ltrim(rtrim(string(p)));
        if (l.size() == 0) continue;
        if (l.find("#") != string::npos) continue;
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

uint find_block(vector<block> &blocks, uint32_t cp)
{
    uint i = 0;
    for (auto &b: blocks) {
        if (cp >= b.start && cp <= b.end) return i;
        i++;
    }
    return blocks.size()-1;
}

void do_print_blocks()
{
    vector<block>  blocks = read_blocks();
    for (auto &b : blocks) {
        printf("%04u-%04u %s\n", b.start, b.end, b.name.c_str());
    }
}

enum class General_Category
{
    Lu, /* Letter, Uppercase */
    Ll, /* Letter, Lowercase */
    Lt, /* Letter, Titlecase */
    Lm, /* Letter, Modifier */
    Lo, /* Letter, Other */
    Mn, /* Mark, Nonspacing */
    Mc, /* Mark, Spacing Combining */
    Me, /* Mark, Enclosing */
    Nd, /*  Number, Decimal Digit */
    Nl, /* Number, Letter */
    No, /* Number, Other */
    Pc, /* Punctuation, Connector */
    Pd, /* Punctuation, Dash */
    Ps, /* Punctuation, Open */
    Pe, /* Punctuation, Close */
    Pi, /* Punctuation, Initial quote */
    Pf, /* Punctuation, Final quote */
    Po, /* Punctuation, Other */
    Sm, /* Symbol, Math */
    Sc, /* Symbol, Currency */
    Sk, /* Symbol, Modifier */
    So, /* Symbol, Other */
    Zs, /* Separator, Space */
    Zl, /* Separator, Line */
    Zp, /* Separator, Paragraph */
    Cc, /* Other, Control */
    Cf, /* Other, Format */
    Cs, /* Other, Surrogate */
    Co, /* Other, Private Use */
    Cn, /* Other, Not Assigned */
};

enum Canonical_Combining_Class {
    Not_Reordered        = 0,   /* Spacing and enclosing marks; also many vowel and consonant signs, even if nonspacing */
    Overlay              = 1,   /* Marks which overlay a base letter or symbol */
    Han_Reading          = 6,   /* Diacritic reading marks for CJK unified ideographs */
    Nukta                = 7,   /* Diacritic nukta marks in Brahmi-derived scripts */
    Kana_Voicing         = 8,   /* Hiragana/Katakana voicing marks */
    Virama               = 9,   /* Viramas */
    Ccc10                = 10,  /* Start of fixed position classes */
    Ccc199               = 199, /* End of fixed position classes */
    Attached_Below_Left  = 200, /* Marks attached at the bottom left */
    Attached_Below       = 202, /* Marks attached directly below */
    Attached_Above       = 214, /* Marks attached directly above */
    Attached_Above_Right = 216, /* Marks attached at the top right */
    Below_Left           = 218, /* Distinct marks at the bottom left */
    Below                = 220, /* Distinct marks directly below */
    Below_Right          = 222, /* Distinct marks at the bottom right */
    Left                 = 224, /* Distinct marks to the left */
    Right                = 226, /* Distinct marks to the right */
    Above_Left           = 228, /* Distinct marks at the top left */
    Above                = 230, /* Distinct marks directly above */
    Above_Right          = 232, /* Distinct marks at the top right */
    Double_Below         = 233, /* Distinct marks subtending two bases */
    Double_Above         = 234, /* Distinct marks extending above two bases */
    Iota_Subscript       = 240, /* Greek iota subscript only */
};

enum class Bidi_Class
{
    L,   /* Left-to-Right */
    LRE, /* Left-to-Right Embedding */
    LRO, /* Left-to-Right Override */
    R,   /* Right-to-Left */
    AL,  /* Right-to-Left Arabic */
    RLE, /* Right-to-Left Embedding */
    RLO, /* Right-to-Left Override */
    PDF, /* Pop Directional Format */
    EN,  /* European Number */
    ES,  /* European Number Separator */
    ET,  /* European Number Terminator */
    AN,  /* Arabic Number */
    CS,  /* Common Number Separator */
    NSM, /* Non-Spacing Mark */
    BN,  /* Boundary Neutral */
    B,   /* Paragraph Separator */
    S,   /* Segment Separator */
    WS,  /* Whitespace */
    ON,  /* Other Neutrals */

    LRI, /* Left-to-Right Isolate */
    FSI, /* First Strong Isolate */
    PDI, /* Pop Directional Isolate */
    RLI, /* Right-to-Left Isolate */
};

/*
 * Decomposition_Type Examples:
 *
 *   0041 0300      (A WITH GRAVE)
 *   0041 0301      (A WITH ACUTE)
 *   <circle> 0030  (CIRCLED DIGIT ZERO)
 */

static const char* Decomposition_Types[] = {
    "",
    "<font>",
    "<noBreak>",
    "<initial>",
    "<medial>",
    "<final>",
    "<isolated> ",
    "<circle>",
    "<super>",
    "<sub>",
    "<vertical>",
    "<wide>",
    "<narrow>",
    "<small>",
    "<square>",
    "<fraction>",
    "<compat>",
    nullptr,
};

enum {
    DT_none,      /* No tag. */
    DT_font,      /* A font variant (e.g. a blackletter form). */
    DT_noBreak,   /* A no-break version of a space or hyphen. */
    DT_initial,   /* An initial presentation form (Arabic). */
    DT_medial,    /* A medial presentation form (Arabic). */
    DT_final,     /* A final presentation form (Arabic). */
    DT_isolated,  /*  An isolated presentation form (Arabic). */
    DT_circle,    /* An encircled form. */
    DT_super,     /* A superscript form. */
    DT_sub,       /* A subscript form. */
    DT_vertical,  /* A vertical layout presentation form. */
    DT_wide,      /* A wide (or zenkaku) compatibility character. */
    DT_narrow,    /* A narrow (or hankaku) compatibility character. */
    DT_small,     /* A small variant form (CNS compatibility). */
    DT_square,    /* A CJK squared font variant. */
    DT_fraction,  /* A vulgar fraction form. */
    DT_compat,    /* Otherwise unspecified compatibility character. */
};

/*
 * Decomposition_Mapping
 * Numeric_Type
 *
 * 0-9
 */

/*
 * Numeric_Value
 *
 * 5000
 * -1/2
 * 5/12
 */

enum Bidi_Mirrored
{
    BM_N,
    BM_Y,
};

/*
 * Simple_Uppercase_Mapping
 * Simple_Lowercase_Mapping
 * Simple_Titlecase_Mapping
 *
 * [0-9A-F]+     (codepoint)
 */


struct data {
    /*  0 */ uint32_t Code;
    /*  1 */ string Name;
    /*  2 */ string General_Category;
    /*  3 */ string Canonical_Combining_Class;
    /*  4 */ string Bidi_Class;
    /*  5 */ string Decomposition_Type;
    /*  6 */ string Decomposition_Mapping;
    /*  7 */ string Numeric_Type;
    /*  8 */ string Numeric_Value;
    /*  9 */ string Bidi_Mirrored;
    /* 10 */ string Unicode_1_Name;
    /* 11 */ string ISO_Comment;
    /* 12 */ string Simple_Uppercase_Mapping;
    /* 13 */ string Simple_Lowercase_Mapping;
    /* 14 */ string Simple_Titlecase_Mapping;
};

/*
 * compression
 */

enum type_enum {
    type_code,
    type_inner,
    type_leaf,
} ;

struct token_node;

union data_union {
    struct {
        size_t code;
        token_node* node;
    } code;
    struct {
        token_node* left;
        token_node* right;
    } inner;
    struct {
        size_t symbol;
    } leaf;

    data_union(size_t code, token_node* node) : code{code, node} {}
    data_union(token_node *left, token_node *right) : inner{left, right} {}
    data_union(size_t symbol) : leaf{symbol} {}
};

struct token_node
{
    type_enum type;
    data_union u;
    size_t node_id;
    bool visited;
};

struct token_set
{
    map<string,size_t> prefix_hist;
    map<size_t,size_t> char_hist;

    size_t next_node_id;
    vector<token_node*> codepoints;
    map<string,token_node*> memo;
    vector<string> symbols;

    static token_node* new_root(size_t code, token_node *node)
    {
        return new token_node{ type_code, data_union{ code, node } };
    }

    static token_node* new_inner(token_node *left, token_node *right)
    {
        return new token_node{ type_inner, data_union{left, right } };
    }

    static token_node* new_leaf(size_t symbol)
    {
        return new token_node{ type_leaf, data_union{ symbol } };
    }

    token_set() : next_node_id(0) {}

    void tokenize(string name)
    {
        vector<string> comps = split(name, " ", "-");
        for (size_t i = 0; i < comps.size(); i++) {
            for (size_t j = i + 1; j <= comps.size(); j++) {
                string prefix = join(comps, " ", i, j);
                prefix_hist[prefix]++;
            }
        }
    }

    token_node* compress(string name)
    {
        token_node *left = nullptr, *right = nullptr;

        vector<string> comps = split(name, " ", "-");

        /* terminal token */
        if (comps.size() == 1) {
            auto ci = memo.find(comps[0]);
            if (ci == memo.end()) {
                size_t symbol = symbols.size();
                symbols.push_back(comps[0]);
                for (auto c : comps[0]) {
                    char_hist[size_t(c)]++;
                }
                left = new_leaf(symbol);
                ci = memo.insert(memo.end(),
                    pair<string,token_node*>(comps[0],left));
            } else {
                left = ci->second;
            }
            return left;
        }

        /* find best prefix */
        size_t idx = 0;
        for (size_t i = 1; i < comps.size(); i++) {
            string prefix = join(comps, " ", 0, i);
            size_t freq = prefix_hist[prefix];
            if (freq > 1) idx = i;
        }
        if (idx == 0 && comps.size() > 0) idx = comps.size() - 1;

        /* memoize prefix */
        string prefix = join(comps, " ", 0, idx);
        auto ci = memo.find(prefix);
        if (ci == memo.end()) {
            left = compress(prefix);
            ci = memo.insert(memo.end(),
                pair<string,token_node*>(prefix,left));
        } else {
            left = ci->second;
        }

        /* memoize suffix */
        if (idx < comps.size()) {
            string suffix = join(comps, " ", idx, comps.size());
            ci = memo.find(suffix);
            if (ci == memo.end()) {
                right = compress(suffix);
                ci = memo.insert(memo.end(),
                    pair<string,token_node*>(suffix,right));
            } else {
                right = ci->second;
            }
        }

        return new_inner(left, right);
    }

    std::string stringify(token_node *node)
    {
        std::string s;
        switch(node->type) {
        case type_code:
            return format("U-%04zx:%s",
                node->u.code.code,
                stringify(node->u.code.node).c_str());
            break;
        case type_inner:
            return format("%zu:{%s,%s}",
                node->node_id,
                stringify(node->u.inner.left).c_str(),
                stringify(node->u.inner.right).c_str());
            break;
        case type_leaf:
            return format("%zu:\"%s\"",
                node->node_id,
                symbols[node->u.leaf.symbol].c_str());
            break;
        }
        return "";
    }

    void traverse_tree(token_node *node, size_t depth = 0)
    {
        std::string indent;
        for (size_t i = 0; i < depth; i++) indent += "  ";

        switch(node->type) {
        case type_code:
            printf("%sU-%04zx\n", indent.c_str(), node->u.code.code);
            traverse_tree(node->u.code.node, depth + 1);
            break;
        case type_inner:
            printf("%sleft {\n", indent.c_str());
            traverse_tree(node->u.inner.left, depth + 1);
            printf("%s}\n", indent.c_str());
            printf("%sright {\n", indent.c_str());
            traverse_tree(node->u.inner.right, depth + 1);
            printf("%s}\n", indent.c_str());
            break;
        case type_leaf:
            printf("%sS-%04zu \"%s\"\n",
                indent.c_str(), node->u.leaf.symbol,
                symbols[node->u.leaf.symbol].c_str());
            break;
        }
    }

    void traverse_tree()
    {
        for (auto n : codepoints) {
            traverse_tree(n);
        }
    }

    void traverse_flat(token_node *node)
    {
        switch(node->type) {
        case type_code:
            if (!node->u.code.node->node_id) {
                traverse_flat(node->u.code.node);
            }
            node->node_id = ++next_node_id;
            printf("code %zu <- %-32s \t# %zd\n",
                node->node_id,
                format("{ U-%04zx, %zd }",
                    node->u.code.code,
                    node->node_id - node->u.code.node->node_id).c_str(),
                node->u.code.node->node_id);
            if (debug_comments) {
                printf("# %s\n", stringify(node).c_str());
            }
            break;
        case type_inner:
            if (!node->u.inner.right->node_id) {
                traverse_flat(node->u.inner.right);
            }
            if (!node->u.inner.left->node_id) {
                traverse_flat(node->u.inner.left);
            }
            node->node_id = ++next_node_id;
            printf("node %zu <- %-32s \t# { %zd, %zd } \n",
                node->node_id,
                format("{ %zd, %zd }",
                    node->node_id - node->u.inner.left->node_id,
                    node->node_id - node->u.inner.right->node_id).c_str(),
                node->u.inner.left->node_id,
                node->u.inner.right->node_id);
            break;
        case type_leaf:
            node->node_id = ++next_node_id;
            printf("leaf %zu <- %-32s \t# %zd\n",
                node->node_id,
                format("{ \"%s\" }",
                    symbols[node->u.leaf.symbol].c_str()).c_str(),
                node->u.leaf.symbol);
            break;
        }
    }

    void traverse_flat()
    {
        for (auto n : codepoints) {
            traverse_flat(n);
        }
    }
};

/*
 * read unicode data
 */

bool parse_codepoint(string str, unsigned long &val)
{
    char *endptr = nullptr;
    val = strtoul(str.c_str(), &endptr, 16);
    return (*endptr == '\0');
}

vector<data> read_data()
{
    // 0000;<control>;Cc;0;BN;;;;;N;NULL;;;;
    vector<data> data;
    FILE *f;
    char buf[256];
    const char* p;

    if ((f = fopen(data_file, "r")) == nullptr) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        exit(1);
    }
    while((p = fgets(buf, sizeof(buf), f)) != nullptr) {
        auto l = ltrim(rtrim(string(p)));
        if (l.size() == 0) continue;
        vector<string> d = split(l, ";", false, true);
        unsigned long codepoint;
        if (!parse_codepoint(d[0], codepoint)) abort();
        data.push_back({
            (uint32_t)codepoint,
            d[1], d[2], d[3], d[4], d[5], d[6], d[7],
            d[8], d[9], d[10], d[11], d[12], d[13], d[14]
        });
    }
    fclose(f);

    return data;
}

void do_print_data()
{
    vector<data> data = read_data();

    for (auto &d : data) {
        printf("%04x\t%s\n", d.Code, d.Name.c_str());
    }
}

template <typename Size, typename Sym>
struct hash_fnv1a
{
    static const Size I = 0xcbf29ce484222325;
    static const Size P = 0x100000001b3;

    Size h;

    hash_fnv1a() : h(I) {}
    inline void add(Sym s) { h ^= s; h *= P; }
    Size hashval() { return h; }
};

struct subhash_ent { int idx; int tok; int offset; int len; };
typedef zedland::hashmap<uint64_t,vector<subhash_ent>> subhash_map;
typedef pair<uint64_t,vector<subhash_ent>> subhash_pair;

static void index_list(subhash_map &index, vector<vector<string>> &lc_tokens, bool debug = false)
{
    for (size_t i = 0; i < lc_tokens.size(); i++) {
        for (size_t j = 0; j < lc_tokens[i].size(); j++) {
            for (size_t k = 0; k < lc_tokens[i][j].size(); k++) {
                hash_fnv1a<uint64_t,char> hf;
                for (size_t l = k; l < lc_tokens[i][j].size(); l++) {
                    hf.add(lc_tokens[i][j][l]);
                    auto ri = index.find(hf.hashval());
                    if (ri == index.end()) {
                        ri = index.insert(subhash_pair(hf.hashval(), vector<subhash_ent>()));
                    }
                    ri->second.push_back(subhash_ent{(int)i,(int)j,(int)k,(int)l-(int)k+1});
                }
            }
        }
    }
    if (!debug) return;
    printf("idx.size()=%zu\n", index.size());
    for (auto it = index.begin(); it != index.end(); it++) {
        uint64_t h = it->first;
        vector<subhash_ent> &l = it->second;
        std::set<string> ss;
        for (size_t j = 0; j < l.size(); j++) {
            subhash_ent e = l[j];
            string s = lc_tokens[e.idx][e.tok].substr(e.offset,e.len);
            ss.insert(s);
        }
        if (ss.size() > 1) {
            printf("collission hval=0x%" PRIx64 "\n", h);
            for (auto si = ss.begin(); si != ss.end(); si++) {
                printf("%s\n", si->c_str());
            }
        }
    }
}

static void do_search_brute_force()
{
    /*
     * load data, tokenize and convert to lower case
     */
    const auto t1 = high_resolution_clock::now();
    vector<data> data = read_data();
    vector<vector<string>> lc_tokens;
    size_t byte_count = 0;
    for (size_t i = 0; i < data.size(); i++) {
        byte_count += data[i].Name.size();
        vector<string> tl = split(data[i].Name.data(), " ", false, false);
        for (auto &name : tl) {
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c){ return std::tolower(c); });
        }
        lc_tokens.push_back(tl);
    }
    const auto t2 = high_resolution_clock::now();

    /*
     * convert search terms to lower case
     */
    vector<string> lc_terms = split(search_data, " ", false, false);
    for (auto &term : lc_terms) {
        std::transform(term.begin(), term.end(), term.begin(),
            [](unsigned char c){ return std::tolower(c); });
    }

    /*
     * brute force search all rows for matches
     */
    for (size_t i = 0; i < data.size(); i++) {
        size_t matches = 0;
        for (auto &lc_term : lc_terms) {
            bool match = false;
            if (lc_term.size() > 1 && lc_term[0] == '\"') {
                bool has_close_quote = lc_term[lc_term.size()-1] == '\"';
                for (auto &lc_token : lc_tokens[i]) {
                    match |= (lc_token == lc_term.substr(1,lc_term.size()-1-has_close_quote));
                }
            } else {
                for (auto &lc_token : lc_tokens[i]) {
                    auto o = lc_token.find(lc_term);
                    match |= (o != std::string::npos);
                }
            }
            if (match) matches++;
        }
        if (matches == lc_terms.size()) {
            char buf[5];
            utf32_to_utf8(buf, sizeof(buf), data[i].Code);
            printf("%s\tU+%04x\t%s\n", buf, data[i].Code, data[i].Name.c_str());
        }
    }

    /*
     * print timings
     */
    const auto t3 = high_resolution_clock::now();
    uint64_t tl = duration_cast<nanoseconds>(t2 - t1).count();
    uint64_t ts = duration_cast<nanoseconds>(t3 - t2).count();
    printf("[Brute-Force] load = %.fμs, search = %.fμs, rows = %zu, bytes = %zu\n",
        (float)tl / 1e3, (uint64_t)ts / 1e3, data.size(), byte_count);
}

static void do_search_rabin_karp()
{
    /*
     * load data, tokenize and convert to lower case
     */
    const auto t1 = high_resolution_clock::now();
    vector<data> data = read_data();
    vector<vector<string>> lc_tokens;
    size_t byte_count = 0;
    for (size_t i = 0; i < data.size(); i++) {
        byte_count += data[i].Name.size();
        vector<string> tl = split(data[i].Name.data(), " ", false, false);
        for (auto &name : tl) {
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c){ return std::tolower(c); });
        }
        lc_tokens.push_back(tl);
    }

    /*
     * create Rabin-Karp substring hash indices
     */
    subhash_map lc_index;
    index_list(lc_index, lc_tokens);
    const auto t2 = high_resolution_clock::now();

    /*
     * convert search terms to lower case
     */
    vector<string> lc_terms = split(search_data, " ", false, false);
    for (auto &term : lc_terms) {
        std::transform(term.begin(), term.end(), term.begin(),
            [](unsigned char c){ return std::tolower(c); });
    }

    /*
     * search the tokenized Rabin-Karp hash table for token matches
     *
     * search result map containing: row -> { term, count }
     */
    map<int,zedland::hashmap<int,int>> results;
    for (size_t term = 0; term < lc_terms.size(); term++)
    {
        auto &lc_term = lc_terms[term];
        hash_fnv1a<uint64_t,int> hf;
        bool needs_exact = false, is_exact = false;;
        if (lc_term.size() > 1 && lc_term[0] == '\"') {
            needs_exact = true;
            bool has_close_quote = lc_term[lc_term.size()-1] == '\"';
            for (size_t i = 1; i < lc_term.size() - has_close_quote; i++) {
                hf.add(lc_term[i]);
            }
        } else {
            for (size_t i = 0; i < lc_term.size(); i++) {
                hf.add(lc_term[i]);
            }
        }
        auto ri = lc_index.find(hf.hashval());
        if (ri == lc_index.end()) continue;

        for (size_t j = 0; j < ri->second.size(); j++) {
            subhash_ent e = ri->second[j];
            string s = lc_tokens[e.idx][e.tok].substr(e.offset,e.len);
            is_exact = (e.offset == 0 && e.len == lc_tokens[e.idx][e.tok].size());
            auto si = results.find(e.idx);
            if (!needs_exact || (needs_exact && is_exact)) {
                if (si == results.end()) {
                    si = results.insert(results.end(),
                        pair<int,zedland::hashmap<int,int>>
                        (e.idx,zedland::hashmap<int,int>()));
                }
                si->second[term]++;
            }
        }
    }

    /*
     * loop through results which are organised by row matched
     *
     * print results for lines where the sum of matches is equal
     * to the number of terms. this means each term must match at
     * least once but can match in the same column more than once.
     * all terms must be covered.
     */
    for (auto ri = results.begin(); ri != results.end(); ri++)
    {
        int i = ri->first;
        if (ri->second.size() == lc_terms.size()) {
            char buf[5];
            utf32_to_utf8(buf, sizeof(buf), data[i].Code);
            printf("%s\tU+%04x\t%s\n", buf, data[i].Code, data[i].Name.c_str());
        }
    }

    /*
     * print timings
     */
    const auto t3 = high_resolution_clock::now();
    uint64_t tl = duration_cast<nanoseconds>(t2 - t1).count();
    uint64_t ts = duration_cast<nanoseconds>(t3 - t2).count();
    printf("[Rabin-Karp] load = %.fμs, search = %.fμs, rows = %zu, bytes = %zu\n",
        (float)tl / 1e3, (uint64_t)ts / 1e3, data.size(), byte_count);
}

/*
 * order0 range encode using adaptive frequencies
 */
template <typename Coder>
static void order0_encode(bitcode_writer &out, vector<size_t> &in, size_t count, FreqTable &t)
{
    Coder c(&out);

    for (size_t i = 0; i < count; i++)
    {
        size_t sym = in[i];
        c.EncodeRange(t.CumFreq[sym], t.CumFreq[sym + 1], t.CumFreq.back());
    }

    c.Flush();
    out.flush();
}

/*
 * order0 range decode using adaptive frequencies
 */
template <typename Coder>
static void order0_decode(vector<size_t> &out, bitcode_reader &in, size_t count, FreqTable &t)
{
    Coder c(&in);

    c.Prime();

    for (size_t i = 0; i < count; i++)
    {
        size_t Count = c.GetCurrentCount(t.CumFreq.back());

        auto si = std::upper_bound(t.CumFreq.begin(), t.CumFreq.end(), Count);
        size_t sym = std::distance(t.CumFreq.begin(), si - 1);

        out[i] = sym;
        c.RemoveRange(t.CumFreq[sym], t.CumFreq[sym + 1], t.CumFreq.back());
    }
}

map<string,size_t> compute_ngram_freq(vector<string> &symbols, size_t ngrams)
{
    map<string,size_t> hist;

    for (auto &sym : symbols) {
        for (size_t j = 1; j <= std::min(sym.size(), (size_t)ngrams); j++) {
            for (size_t i = 0; i < sym.size() - j + 1; i++) {
                hist[sym.substr(i, j)]++;
            }
        }
    }

    return hist;
}

vector<pair<string,size_t>> sort_filter_ngram(map<string,size_t> hist, size_t count)
{
    vector<pair<string,size_t>> ngrams;
    std::transform(hist.begin(), hist.end(), std::back_inserter(ngrams),
        [](const decltype(hist)::value_type& val){ return val; } );
    std::sort(ngrams.begin(), ngrams.end(), [](const vector<pair<string,size_t>>::value_type& a,
        const vector<pair<string,size_t>>::value_type& b) {
        size_t ra = (a.first.size() == 1) ? 65536 : a.first.size();
        size_t rb = (b.first.size() == 1) ? 65536 : b.first.size();
        return a.second *ra > b.second * rb;
    });
    size_t z = 0;
    for (auto i = ngrams.begin(); i != ngrams.end();) {
        if (i->first.size() == 2) i = ngrams.erase(i);
        else if (z++ < count) i++;
        else i = ngrams.erase(i);
    }
    return ngrams;
}

static size_t sym_delimeter = 0;
static size_t sym_offset = 1;

void write_dict(bitcode_writer &out, vector<pair<string,size_t>> &ngrams, vector<string> &symbols, bool include_freq)
{
    /*
        n-grams need to be sorted by size, so that when we write them out,
        we just write out the count of each size class. This means we don't
        need to write delimeters. This functions depends on pre-sorted ngrams.
    */

    std::sort(ngrams.begin(), ngrams.end(), [](const vector<pair<string,size_t>>::value_type& a,
        const vector<pair<string,size_t>>::value_type& b) { return a.first.size() < b.first.size() ||
        ((a.first.size() == b.first.size()) && (a.second > b.second)); });

    /* count distinct sizes ngram sizes */
    std::vector<size_t> sizes;
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        size_t sz = i->first.size();
        if (sizes.size() < sz) sizes.resize(sz);
        sizes[sz-1]++;
    }

    /* create map of ngram symbol indices */
    size_t ngram_length = 0;
    map<string,size_t> ngram_map;
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        ngram_map[i->first] = std::distance(ngrams.begin(), i);
        ngram_length = std::max(ngram_length, i->first.size());
    }

    /* decompose symbols into ngrams */
    std::vector<size_t> stream;
    std::vector<size_t> freq;
    freq.resize(ngrams.size() + sym_offset);
    for (auto sym : symbols) {
        for (size_t i = 0; i < sym.size(); ) {
            ssize_t symbol = -1;
            for (size_t j = std::min(sym.size() - i, ngram_length); j > 0; j--) {
                auto si = ngram_map.find(sym.substr(i,j));
                if (si == ngram_map.end()) continue;
                symbol = si->second;
                break;
            }
            assert(symbol != -1);
            i += ngrams[symbol].first.size();
            stream.push_back(symbol + sym_offset);
            //freq[symbol + sym_offset] += ngrams[symbol].first.size();
            freq[symbol + sym_offset]++;
        }
        stream.push_back(sym_delimeter); /* delimeter */
        freq[sym_delimeter]++;
    }
    printf("symbol_count: %zu symbols\n", symbols.size());
    printf("stream_count: %zu symbols\n", stream.size());

    /* write count of ngram size counts */
    out.write_vlu(sizes.size());

    /* write ngram size counts */
    for (auto sz : sizes) {
        out.write_vlu(sz);
    }

    /* write ngrams followed by their frequencies */
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        for (auto c : i->first) {
            out.write_vlu(c);
        }
        if (include_freq) {
            //out.write_vlu(i->second);
            out.write_vlu(freq[std::distance(ngrams.begin(),i) + sym_offset]);
            //out.write_vlu(i->first.size());
        }
    }

    /* construct ngram frequency table */
    FreqTable t(ngrams.size() + sym_offset + 1);
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        size_t symbol = std::distance(ngrams.begin(), i);
        if (include_freq) {
            t.Freq[symbol + sym_offset] = freq[symbol + sym_offset];
            //t.Freq[symbol + sym_offset] = i->first.size();
            //t.Freq[symbol + sym_offset] = 1;
        } else {
            t.Freq[symbol + sym_offset] = 1;
        }
    }

    /* write symbol table */
    size_t symbol_count = symbols.size();
    size_t stream_count = stream.size();
    out.write_vlu(symbol_count);
    out.write_vlu(stream_count);

    t.Freq[sym_delimeter] = include_freq ? symbol_count : 1;
    t.ToCumulative(RangeCoder32::MaxRange);
#if 0
    for (size_t i = 0; i < t.Freq.size(); i++) {
        printf("[%3zu] %3s = %5zu, %5zu     ",
            i, (i >= sym_offset) ? ngrams[i-sym_offset].first.c_str() : "",
            t.Freq[i], t.CumFreq[i]);
        if (i % 5 == 0) printf("\n");
    }
    printf("\n");
    abort();
#endif
    order0_encode<RangeCoder32>(out, stream, stream_count, t);
}

void read_dict(bitcode_reader &in, vector<pair<string,size_t>> &ngrams, vector<string> &symbols, bool include_freq)
{
    std::vector<size_t> sizes;

    /* read count of ngram size counts */
    size_t ngram_sizes = in.read_vlu();
    printf("ngram sizes: %zu\n", ngram_sizes);

    /* read ngram size counts */
    for (size_t i = 0; i < ngram_sizes; i++) {
        size_t ngram_size = in.read_vlu();
        sizes.push_back(ngram_size);
        printf("ngram size[%zu]: %zu\n", i + 1, ngram_size);
    }

    /* read ngrams and their frequencies. note: we don't require delimeters
       because we know the count for each size class. */
    for (size_t i = 0; i < ngram_sizes; i++) {
        size_t sz = sizes[i];
        for (size_t j = 0; j < sz; j++) {
            std::string ngram;
            for (size_t k = 0; k < i+1; k++) {
                int c = in.read_vlu();
                ngram.append(1, c);
            }
            if (include_freq) {
                size_t freq = in.read_vlu();
                ngrams.push_back(std::pair<string,size_t>(ngram, freq));
            } else {
                ngrams.push_back(std::pair<string,size_t>(ngram, 1));
            }
        }
    }

    /* print to verify */
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        if (distance(ngrams.begin(),i) % 16 == 0) printf("\n");
        printf("%-5s%4zu ", i->first.c_str(), i->second);
    }
    printf("\n\n");

    /* construct ngram frequency table */
    FreqTable t(ngrams.size() + sym_offset + 1);
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        t.Freq[std::distance(ngrams.begin(),i) + sym_offset] = i->second;
        //t.Freq[std::distance(ngrams.begin(),i) + sym_offset] = 1;
        //t.Freq[std::distance(ngrams.begin(),i) + sym_offset] = i->first.size();
    }

    /* read symbol table */
    vector<size_t> stream;
    size_t symbol_count = in.read_vlu();
    size_t stream_count = in.read_vlu();
    stream.resize(stream_count);

    t.Freq[sym_delimeter] = include_freq ? symbol_count : 1;
    t.ToCumulative(RangeCoder32::MaxRange);
    order0_decode<RangeCoder32>(stream, in, stream_count, t);

    printf("symbol_count: %zu (# symbols)\n", symbol_count);
    printf("stream_count: %zu (# ngrams)\n", stream_count);

    /* reconstruct encoded symbols */
    std::string str;
    for (size_t i = 0; i < stream.size(); i++) {
        size_t symbol = stream[i];
        if (symbol == sym_delimeter /* delimeter */) {
            symbols.push_back(str);
            str.clear();
        } else if (symbol >= sym_offset) {
            str += ngrams[symbol - sym_offset].first;
        } else {
            printf("unknown symbol: %zu\n", symbol);
        }
    }

    /* print to verify */
    if (debug_symbols) {
        for (auto i = symbols.begin(); i != symbols.end(); i++) {
            if (distance(symbols.begin(),i) % 8 == 0) printf("\n");
            printf("%-20s", i->c_str());
        }
        printf("\n\n");
    }
}

void do_compress_stats()
{
    token_set ts;
    vector<data> data = read_data();

    for (auto &d : data) {
        ts.tokenize(d.Name);
    }
    for (auto &d : data) {
        ts.codepoints.push_back(ts.new_root(d.Code, ts.compress(d.Name)));
    }

    string s;
    for (auto &c : ts.char_hist) {
        s.append(1, c.first);
    }

    size_t total_chars = 0;
    for (auto &d : data) {
        total_chars += d.Name.size() + 1;
    }

    std::sort(ts.symbols.begin(), ts.symbols.end());
    std::string symbols = join(ts.symbols, " ", 0, ts.symbols.size());

    printf("character set     : %s\n", s.c_str());
    printf("character count   : %zi\n", s.size());
    printf("symbol count      : %zu\n", ts.symbols.size());
    printf("symbol table size : %zu bytes\n", symbols.size());
    printf("total size        : %zu byes\n", total_chars);

    if (debug_symbols) {
        printf("\n%s\n", symbols.c_str());
    }

    map<string,size_t> hist = compute_ngram_freq(ts.symbols, 3);
    vector<pair<string,size_t>> ngrams = sort_filter_ngram(hist, 255);

    printf("\nunigram and bigram frequency\n");
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        if (distance(ngrams.begin(),i) % 16 == 0) printf("\n");
        printf("%-5s%4zu ", i->first.c_str(), i->second);
    }
    printf("\n\n");

    const bool include_freq = true;

    vector_writer vw;
    bitcode_writer bw(&vw);

    write_dict(bw, ngrams, ts.symbols, include_freq);
    bw.flush();
    printf("dict_size: %zu bytes\n", vw.buffer.size());

    vector_reader vr;
    bitcode_reader br(&vr);
    vr.set(vw.buffer);

    vector<pair<string,size_t>> r_ngrams;
    vector<string> r_symbols;
    read_dict(br, r_ngrams, r_symbols, include_freq);

    /*
        Some analysis of Unicode Character Name Romanisations. They use
        60 characters (~6 bits): Latin upper-case, lower-case, the comma,
        the hyphen, and the greater than and less than signs used as
        <meta quotation marks>. Only the meta text contains lower-case,
        so mainly 34 characters.
    */
    /*
     * text    (size) : 873997
     * symbols (size) : 76532
     *
     * codes  (count) : 32841
     * leafs  (count) : 13006
     * inner  (count) : 42116
     * nodes  (total) : 87963
     *
     * symbols          = 76532
     * codes  32841 * 1 = 32841   # almost all are last_char_last_node
     * inner  42116 * 4 = 168464  # most are 3 to 4 bytes
     * leafs  13006 * 1 = 13006
     *        (total)   = 290843
     */
}

void do_compress_data()
{
    token_set ts;
    vector<data> data = read_data();

    for (auto &d : data) {
        ts.tokenize(d.Name);
        //ts.tokenize(d.Unicode_1_Name);
    }
    for (auto &d : data) {
        ts.codepoints.push_back(ts.new_root(d.Code, ts.compress(d.Name)));
        //ts.codepoints.push_back(ts.new_root(d.Code, ts.compress(d.Unicode_1_Name)));
    }
    if (debug_tree) {
        ts.traverse_tree();
    }
    if (debug_flat) {
        ts.traverse_flat();
    }
}

void do_experiment()
{
    token_set ts;
    vector<data> data = read_data();

    for (auto &d : data) {
        ts.tokenize(d.Name);
    }
    for (auto &d : data) {
        ts.codepoints.push_back(ts.new_root(d.Code, ts.compress(d.Name)));
    }

    map<string,size_t> hist = compute_ngram_freq(ts.symbols, 3);
    vector<pair<string,size_t>> ngrams = sort_filter_ngram(hist, 255);

    printf("\nunigram and bigram frequency\n");
    for (auto i = ngrams.begin(); i != ngrams.end(); i++) {
        if (distance(ngrams.begin(),i) % 16 == 0) printf("\n");
        printf("%-5s%4zu ", i->first.c_str(), i->second);
    }
    printf("\n\n");
}

/*
 * command line options
 */

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -u, --data-file <name>       unicode data file\n"
        "  -b, --blocks-file <name>     unicode blocks file\n"
        "  -p, --print-data             print unicode data\n"
        "  -s, --search <string>        search unicode data\n"
        "  -x, --brute-force            disable search optimization\n"
        "  -B, --print-blocks           print unicode blocks\n"
        "  -S, --debug-symbols          compress debug symbols\n"
        "  -T, --debug-tree             compress debug tree\n"
        "  -F, --debug-flat             compress debug flat\n"
        "  -C, --debug-comments         compress debug comments\n"
        "  -z, --compress-data          compression\n"
        "  -Z, --compress-stats         compression stats\n"
        "  -e, --experiment             experiment\n"
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
        if (match_opt(argv[i], "-u", "--data-file")) {
            if (check_param(++i == argc, "--data-file")) break;
            data_file = argv[i++];
        } else if (match_opt(argv[i], "-b", "--blocks-file")) {
            if (check_param(++i == argc, "--blocks-file")) break;
            blocks_file = argv[i++];
        } else if (match_opt(argv[i], "-p", "--print-data")) {
            print_data = true;
            i++;
        } else if (match_opt(argv[i], "-s", "--search")) {
            if (check_param(++i == argc, "--search")) break;
            search_data = argv[i++];
        } else if (match_opt(argv[i], "-x", "--brute-force")) {
            optimized_search = false;
            i++;
        } else if (match_opt(argv[i], "-B", "--print-blocks")) {
            print_blocks = true;
            i++;
        } else if (match_opt(argv[i], "-Z", "--compress-stats")) {
            compress_stats = true;
            i++;
        } else if (match_opt(argv[i], "-S", "--debug-symbols")) {
            debug_symbols = true;
            i++;
        } else if (match_opt(argv[i], "-T", "--debug-tree")) {
            debug_tree = true;
            i++;
        } else if (match_opt(argv[i], "-F", "--debug-flat")) {
            debug_flat = true;
            i++;
        } else if (match_opt(argv[i], "-C", "--debug-comments")) {
            debug_comments = true;
            i++;
        } else if (match_opt(argv[i], "-z", "--compress-data")) {
            compress_data = true;
            i++;
        } else if (match_opt(argv[i], "-e", "--experiment")) {
            experiment = true;
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

/*
 * main program
 */

int main(int argc, char **argv)
{
    parse_options(argc, argv);

    if (print_data) do_print_data();
    if (search_data)
        if (optimized_search)
            do_search_rabin_karp();
        else
            do_search_brute_force();
    if (print_blocks) do_print_blocks();
    if (compress_stats) do_compress_stats();
    if (compress_data) do_compress_data();
    if (experiment) do_experiment();
}
