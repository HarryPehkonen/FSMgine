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