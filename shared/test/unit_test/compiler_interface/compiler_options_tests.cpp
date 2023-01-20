/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/test/common/test_macros/test.h"

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

    constexpr NEO::ConstStringRef toConcatenate[] = {"a", "b", "c"};
    constexpr ConstConcatenation<concatenationLength(toConcatenate)> constConcatenationSpecificSize(toConcatenate);
    constexpr ConstConcatenation<> constConcatenationDefaultSize(toConcatenate);
    EXPECT_TRUE(NEO::ConstStringRef("a b c") == constConcatenationSpecificSize);
    EXPECT_TRUE(NEO::ConstStringRef("a b c") == constConcatenationDefaultSize);
    EXPECT_TRUE(constConcatenationSpecificSize == NEO::ConstStringRef("a b c"));
    EXPECT_TRUE(constConcatenationDefaultSize == NEO::ConstStringRef("a b c"));
}

TEST(CompilerOptions, WhenConcatenateAppendIsCalledThenAddsSpaceAsSeparatorOnlyIfMissing) {
    using namespace NEO::CompilerOptions;
    std::string concatenated = NEO::CompilerOptions::optDisable.data();
    concatenateAppend(concatenated, NEO::CompilerOptions::finiteMathOnly);
    auto expected = (NEO::CompilerOptions::optDisable.str() + " " + NEO::CompilerOptions::finiteMathOnly.data());
    EXPECT_STREQ(expected.c_str(), concatenated.c_str());
    concatenated += " ";
    concatenateAppend(concatenated, NEO::CompilerOptions::fastRelaxedMath);
    expected += " ";
    expected += NEO::CompilerOptions::fastRelaxedMath.data();
    EXPECT_STREQ(expected.c_str(), concatenated.c_str());
}

TEST(CompilerOptions, WhenTryingToExtractNonexistentOptionThenFalseIsReturnedAndStringIsNotModified) {
    const std::string optionsInput{"-ze-allow-zebin -cl-intel-has-buffer-offset-arg"};

    std::string options{optionsInput};
    const bool wasExtracted{NEO::CompilerOptions::extract(NEO::CompilerOptions::noRecompiledFromIr, options)};

    EXPECT_FALSE(wasExtracted);
    EXPECT_EQ(optionsInput, options);
}

TEST(CompilerOptions, WhenTryingToExtractOptionThatExistsThenTrueIsReturnedAndStringIsModified) {
    const std::string optionsInput{"-ze-allow-zebin -Wno-recompiled-from-ir -cl-intel-has-buffer-offset-arg"};

    std::string options{optionsInput};
    const bool wasExtracted{NEO::CompilerOptions::extract(NEO::CompilerOptions::noRecompiledFromIr, options)};

    EXPECT_TRUE(wasExtracted);

    const std::string expectedOptions{"-ze-allow-zebin  -cl-intel-has-buffer-offset-arg"};
    EXPECT_EQ(expectedOptions, options);
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
