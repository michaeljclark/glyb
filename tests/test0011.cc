#undef NDEBUG
#include <cassert>
#include <cstring>

#include "utf8.h"

#include <initializer_list>

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