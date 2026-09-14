#include <gtest/gtest.h>
#include "FSMgine/StringInterner.hpp"

using namespace fsmgine;

class StringInternerTest : public ::testing::Test {
protected:
    void SetUp() override {
        StringInterner::instance().clear();
    }
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

TEST_F(StringInternerTest, ClearFunctionality) {
    auto& interner = StringInterner::instance();
    
    auto view1 = interner.intern(std::string("test"));
    interner.clear();
    auto view2 = interner.intern(std::string("test"));
    
    // After clear, same string should be re-interned
    EXPECT_EQ(view1, view2); // Same content
    // Note: Memory address may or may not be the same after clear
}

// --- arena accounting: reset() vs clear() ----------------------------------
// clear() forgets the lookup index but KEEPS the storage arena, so views handed
// out earlier stay valid. reset() releases the arena, which invalidates them.
// arena_size() makes that difference observable (and is what the fuzzer uses to
// hold memory flat between inputs without giving up long state names).

TEST_F(StringInternerTest, ArenaSizeCountsRetainedStrings) {
    auto& interner = StringInterner::instance();

    const std::size_t before = interner.arena_size();

    interner.intern(std::string("arena_alpha_zz1"));
    interner.intern(std::string("arena_beta_zz2"));
    interner.intern(std::string("arena_alpha_zz1")); // cache hit: not retained again

    EXPECT_EQ(interner.arena_size(), before + 2u);
}

TEST_F(StringInternerTest, ClearKeepsTheArenaSoOldViewsStayValid) {
    auto& interner = StringInterner::instance();

    const auto view = interner.intern(std::string("arena_keeper_zz3"));
    const std::size_t before = interner.arena_size();

    interner.clear();

    EXPECT_EQ(interner.arena_size(), before); // clear() forgets the index only
    EXPECT_EQ(view, "arena_keeper_zz3");      // and the old view is still valid
}

TEST_F(StringInternerTest, ResetReleasesTheArena) {
    auto& interner = StringInterner::instance();

    interner.intern(std::string("arena_one_zz4"));
    interner.intern(std::string("arena_two_zz5"));
    ASSERT_GT(interner.arena_size(), 0u);

    interner.reset();

    EXPECT_EQ(interner.arena_size(), 0u);
}

TEST_F(StringInternerTest, InterningAfterResetYieldsValidContentsAgain) {
    auto& interner = StringInterner::instance();
    const std::string text = "arena_phoenix_zz6";

    const auto before = interner.intern(text);
    const std::string before_contents(before); // read it while it is still valid

    interner.reset(); // `before` is dangling from here on — never touch it again

    const auto after = interner.intern(text);

    EXPECT_EQ(after, text);
    EXPECT_EQ(before_contents, after);
    EXPECT_FALSE(after.empty());
    EXPECT_NE(after.data(), nullptr);
    // The address is deliberately not asserted: whether the fresh arena reuses the
    // freed block is the allocator's business.
}