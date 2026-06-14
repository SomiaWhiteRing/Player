#include "doctest.h"
#include "pinyin_input.h"

#include <algorithm>

TEST_SUITE_BEGIN("PinyinInput");

TEST_CASE("Lookup returns common Chinese candidates") {
	auto candidates = PinyinInput::Lookup("ni");

	REQUIRE_FALSE(candidates.empty());
	CHECK_EQ(candidates[0], u8"你");
}

TEST_CASE("Lookup is case insensitive") {
	auto lower = PinyinInput::Lookup("zhong");
	auto upper = PinyinInput::Lookup("ZHONG");

	REQUIRE_FALSE(lower.empty());
	REQUIRE_FALSE(upper.empty());
	CHECK_EQ(lower[0], u8"中");
	CHECK_EQ(upper[0], u8"中");
}

TEST_CASE("Lookup supports v for lv syllables") {
	auto candidates = PinyinInput::Lookup("lv");

	REQUIRE_FALSE(candidates.empty());
	CHECK_EQ(candidates[0], u8"率");
}

TEST_CASE("Lookup returns phrase candidates") {
	auto candidates = PinyinInput::Lookup("nihao");

	REQUIRE_FALSE(candidates.empty());
	CHECK_EQ(candidates[0], u8"你好");

	candidates = PinyinInput::Lookup("zhongguo");
	REQUIRE_FALSE(candidates.empty());
	CHECK_EQ(candidates[0], u8"中国");
}

TEST_CASE("Lookup includes uncommon dictionary characters") {
	auto candidates = PinyinInput::Lookup("min");

	CHECK(std::find(candidates.begin(), candidates.end(), u8"旻") != candidates.end());
}

TEST_CASE("Lookup composes candidates from syllables") {
	auto candidates = PinyinInput::Lookup("cangmin");

	CHECK(std::find(candidates.begin(), candidates.end(), u8"苍旻") != candidates.end());
}

TEST_CASE("Lookup returns empty for unknown syllables") {
	CHECK(PinyinInput::Lookup("abc").empty());
	CHECK(PinyinInput::Lookup("").empty());
}

TEST_SUITE_END();
