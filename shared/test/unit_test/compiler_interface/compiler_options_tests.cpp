/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "compiler_options.h"

TEST(CompilerOptions, WhenConcatenationLengthIsCalledThenReturnsSumOfLengthsAndSeperators) {
    using namespace NEO::CompilerOptions;
    constexpr auto concatenatedLength = concatenationLength(NEO::CompilerOptions::optDisable);
    static_assert(optDisable.length() == concatenatedLength, "");
    static_assert(optDisable.length() + 1 + gtpinRera.length() == concatenationLength(optDisable, gtpinRera), "");
    static_assert(optDisable.length() + 1 + gtpinRera.length() + 1 + finiteMathOnly.length() == concatenationLength(optDisable, gtpinRera, finiteMathOnly), "");
}

TEST(CompilerOptions, WhenConcatenateIsCalledThenUsesSpaceAsSeparator) {
    using namespace NEO::CompilerOptions;
    auto concatenated = concatenate(NEO::CompilerOptions::optDisable, NEO::CompilerOptions::finiteMathOnly);
    auto expected = (std::string(NEO::CompilerOptions::optDisable) + " " + NEO::CompilerOptions::finiteMathOnly.data());
    EXPECT_STREQ(expected.c_str(), concatenated.c_str());

    constexpr ConstStringRef toConcatenate[] = {"a", "b", "c"};
    constexpr ConstConcatenation<concatenationLength(toConcatenate)> constConcatenationSpecificSize(toConcatenate);
    constexpr ConstConcatenation<> constConcatenationDefaultSize(toConcatenate);
    EXPECT_TRUE(ConstStringRef("a b c") == constConcatenationSpecificSize);
    EXPECT_TRUE(ConstStringRef("a b c") == constConcatenationDefaultSize);
}

TEST(CompilerOptions, WhenConcatenateAppendIsCalledThenAddsSpaceAsSeparatorOnlyIfMissing) {
    using namespace NEO::CompilerOptions;
    std::string concatenated = NEO::CompilerOptions::optDisable.data();
    concatenateAppend(concatenated, NEO::CompilerOptions::finiteMathOnly);
    auto expected = (std::string(NEO::CompilerOptions::optDisable) + " " + NEO::CompilerOptions::finiteMathOnly.data());
    EXPECT_STREQ(expected.c_str(), concatenated.c_str());
    concatenated += " ";
    concatenateAppend(concatenated, NEO::CompilerOptions::fastRelaxedMath);
    expected += " ";
    expected += NEO::CompilerOptions::fastRelaxedMath;
    EXPECT_STREQ(expected.c_str(), concatenated.c_str());
}

TEST(CompilerOptions, WhenCheckingForPresenceOfOptionThenRejectsSubstrings) {
    EXPECT_FALSE(NEO::CompilerOptions::contains("aaa", "a"));
    EXPECT_FALSE(NEO::CompilerOptions::contains("aaa", "aa"));
    EXPECT_TRUE(NEO::CompilerOptions::contains("aaa", "aaa"));
    EXPECT_FALSE(NEO::CompilerOptions::contains("aaa", "aaaa"));
    EXPECT_TRUE(NEO::CompilerOptions::contains("aaaa aaa", "aaaa"));
    EXPECT_TRUE(NEO::CompilerOptions::contains("aa aaaa", "aaaa"));
}

TEST(CompilerOptions, WhenTokenizingThenSpaceIsUsedAsSeparator) {
    auto tokenizedEmpty = NEO::CompilerOptions::tokenize("");
    EXPECT_TRUE(tokenizedEmpty.empty());

    auto tokenizedOne = NEO::CompilerOptions::tokenize("abc");
    ASSERT_EQ(1U, tokenizedOne.size());
    EXPECT_EQ("abc", tokenizedOne[0]);

    auto tokenizedOneSkipSpaces = NEO::CompilerOptions::tokenize("  abc ");
    ASSERT_EQ(1U, tokenizedOneSkipSpaces.size());
    EXPECT_EQ("abc", tokenizedOneSkipSpaces[0]);

    auto tokenizedMultiple = NEO::CompilerOptions::tokenize("  -optA -optB c ");
    ASSERT_EQ(3U, tokenizedMultiple.size());
    EXPECT_EQ("-optA", tokenizedMultiple[0]);
    EXPECT_EQ("-optB", tokenizedMultiple[1]);
    EXPECT_EQ("c", tokenizedMultiple[2]);
}
