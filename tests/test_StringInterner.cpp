#include "FSMgine/StringInterner.hpp"
#include <gtest/gtest.h>

using namespace fsmgine;

class StringInternerTest : public ::testing::Test {
protected:
    void SetUp() override { StringInterner::instance().resetArena(); }
};

TEST_F(StringInternerTest, BasicInternString) {
    auto& interner = StringInterner::instance();

    std::string str = "test_state";
    auto view1 = interner.intern(str);
    auto view2 = interner.intern(str);

    EXPECT_EQ(view1, view2);
    EXPECT_EQ(view1.data(), view2.data()); // Same memory location
}

TEST_F(StringInternerTest, BasicInternStringView) {
    auto& interner = StringInterner::instance();

    std::string_view sv = "test_state";
    auto view1 = interner.intern(sv);
    auto view2 = interner.intern(sv);

    EXPECT_EQ(view1, view2);
    EXPECT_EQ(view1.data(), view2.data()); // Same memory location
}

TEST_F(StringInternerTest, MixedStringAndStringView) {
    auto& interner = StringInterner::instance();

    std::string str = "test_state";
    std::string_view sv = "test_state";

    auto view1 = interner.intern(str);
    auto view2 = interner.intern(sv);

    EXPECT_EQ(view1, view2);
    EXPECT_EQ(view1.data(), view2.data()); // Same memory location
}

TEST_F(StringInternerTest, DifferentStringsHaveDifferentAddresses) {
    auto& interner = StringInterner::instance();

    auto view1 = interner.intern(std::string("state1"));
    auto view2 = interner.intern(std::string("state2"));

    EXPECT_NE(view1, view2);
    EXPECT_NE(view1.data(), view2.data()); // Different memory locations
}

TEST_F(StringInternerTest, SingletonBehavior) {
    auto& interner1 = StringInterner::instance();
    auto& interner2 = StringInterner::instance();

    EXPECT_EQ(&interner1, &interner2);
}

// --- arena accounting: interning appends, resetArena() releases everything -------
// arenaSize() makes that observable (and is what the fuzzer uses to hold memory flat
// between inputs without giving up long state names).

TEST_F(StringInternerTest, EarlierViewsStayValidAsTheArenaGrows) {
    auto& interner = StringInterner::instance();

    const auto first = interner.intern(std::string("arena_first_zz7"));
    const std::size_t before = interner.arenaSize();

    for (int i = 0; i < 64; ++i) {
        interner.intern(std::string("arena_filler_") + std::to_string(i));
    }

    EXPECT_GT(interner.arenaSize(), before);
    EXPECT_EQ(first, "arena_first_zz7"); // still valid, still correct
}

TEST_F(StringInternerTest, ArenaSizeCountsRetainedStrings) {
    auto& interner = StringInterner::instance();

    const std::size_t before = interner.arenaSize();

    interner.intern(std::string("arena_alpha_zz1"));
    interner.intern(std::string("arena_beta_zz2"));
    interner.intern(std::string("arena_alpha_zz1")); // cache hit: not retained again

    EXPECT_EQ(interner.arenaSize(), before + 2u);
}

TEST_F(StringInternerTest, ResetArenaReleasesTheArena) {
    auto& interner = StringInterner::instance();

    interner.intern(std::string("arena_one_zz4"));
    interner.intern(std::string("arena_two_zz5"));
    ASSERT_GT(interner.arenaSize(), 0u);

    interner.resetArena();

    EXPECT_EQ(interner.arenaSize(), 0u);
}

TEST_F(StringInternerTest, InterningAfterResetArenaYieldsValidContentsAgain) {
    auto& interner = StringInterner::instance();
    const std::string text = "arena_phoenix_zz6";

    const auto before = interner.intern(text);
    const std::string before_contents(before); // read it while it is still valid

    interner.resetArena(); // `before` is dangling from here on — never touch it again

    const auto after = interner.intern(text);

    EXPECT_EQ(after, text);
    EXPECT_EQ(before_contents, after);
    EXPECT_FALSE(after.empty());
    EXPECT_NE(after.data(), nullptr);
    // The address is deliberately not asserted: whether the fresh arena reuses the
    // freed block is the allocator's business.
}