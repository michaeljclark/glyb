#undef NDEBUG
#include <cassert>
#include <cstring>

#include "utf8.h"

#include <initializer_list>


enum {
	emoji_block = 0x1F000,
	emoji_mask = ~0x00fff,
	emoji_flag = 0x1,
};

struct utf8_range { uint64_t off; uint32_t len; uint32_t flags; };

/*
 * utf8_ranges_from_text
 *
 * scan text and return ranges of characters matching unicode blocks
 *
 * @param text to scan
 * @param length of text to scan
 * @param code to match on and split
 * @param mask to match on and split
 * @param flag to add to ranges that include the code
 */

std::vector<utf8_range> utf8_ranges_from_text(const char* text, size_t length,
    uint32_t code, uint32_t mask, uint32_t flag)
{
    std::vector<utf8_range> vec;

    size_t i = 0, j = 0;
    bool last = false;
    while (i < length)
    {
        utf32_code cp = utf8_to_utf32_code(text + i);
        bool match = (cp.code & mask) == code;
        if (i != 0 && last != match || i == UINT_MAX ) {
            vec.push_back({ j, uint32_t(i - j), flag&-(uint32_t)last });
            j = i;
        }
        last = match;
        i += cp.len;
    }
    if (i - j > 0) {
        vec.push_back({ j, uint32_t(i - j), flag&-(uint32_t)last });
    }

    return vec;
}

/*
 * test splittng strings into ranges of emoji and non-emoji characters
 */

void test_find_emoji_ranges(size_t items, const char *text,
	std::initializer_list<std::pair<int,int>> l)
{
	auto vec = utf8_ranges_from_text(text, strlen(text),
		emoji_block, emoji_mask, emoji_flag);

	auto li = l.begin();
	for (auto &ent : vec) {
		assert(ent.off == li->first);
		assert(ent.len == li->second);
		li++;
	}
	assert(vec.size() == items);
}

int main(int argc, char **argv)
{
	test_find_emoji_ranges(0, "", {});
	test_find_emoji_ranges(1, "hello", {{0,5}});
	test_find_emoji_ranges(1, "🙃😙", {{0,8}});
	test_find_emoji_ranges(2, "hello🙃😙😃", {{0,5},{5,12}});
	test_find_emoji_ranges(2, "🙃😙😃😜😍hello", {{0,20},{20,5}});
	test_find_emoji_ranges(3, "hello😍hello", {{0,5},{5,4},{9,5}});
}