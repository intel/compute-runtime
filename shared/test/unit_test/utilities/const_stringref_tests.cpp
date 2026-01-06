/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/const_stringref.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(ConstStringRef, WhenCreatingFromConstantArrayThenIsConstexprAndContainsAllArrayElements) {
    static constexpr ConstStringRef str0 = ConstStringRef::fromArray("some_text");
    static_assert(10U == str0.length(), "");
    static_assert(10U == str0.size(), "");
    static_assert(false == str0.empty(), "");
    static_assert('s' == str0[0], "");
    static_assert('o' == str0[1], "");
    static_assert('m' == str0[2], "");
    static_assert('e' == str0[3], "");
    static_assert('_' == str0[4], "");
    static_assert('t' == str0[5], "");
    static_assert('e' == str0[6], "");
    static_assert('x' == str0[7], "");
    static_assert('t' == str0[8], "");

    static constexpr ConstStringRef str1("second", 3);
    static_assert(3 == str1.length(), "");
    static_assert(3 == str1.size(), "");
    static_assert(false == str1.empty(), "");
    static_assert('s' == str1[0], "");
    static_assert('e' == str1[1], "");
    static_assert('c' == str1[2], "");

    static_assert('s' == str1.data()[0], "");
    static_assert('s' == *str1.begin(), "");
    static_assert(3 == str1.end() - str1.begin(), "");
    static_assert(str1.begin() == str1.data(), "");

    static constexpr ConstStringRef strEmpty("aaa", 0);
    static_assert(0U == strEmpty.length(), "");
    static_assert(0U == strEmpty.size(), "");
    static_assert(strEmpty.empty(), "");

    static_assert(10 == str0.length(), "");

    static_assert(str0 == str0, "");
    static_assert(str1 != str0, "");

    static constexpr ConstStringRef strAbc("abc");
    static constexpr ConstStringRef strAbd("abd");
    static constexpr ConstStringRef strAbc2("abcdef", 3);
    static_assert(strAbc != strAbd, "");
    static_assert(strAbc.length() == strAbc2.length(), "");
    static_assert(strAbc == strAbc2, "");
}

TEST(ConstStringRef, WhenComparingAgainstContainersThenUsesLexicographicOrdering) {
    static constexpr ConstStringRef constStrText("Text");
    std::string strText("Text");
    std::string strToxt("Toxt");
    EXPECT_TRUE(strText == constStrText);
    EXPECT_TRUE(constStrText == strText);
    EXPECT_FALSE(strToxt == constStrText);
    EXPECT_FALSE(constStrText == strToxt);

    EXPECT_FALSE(strText != constStrText);
    EXPECT_FALSE(constStrText != strText);
    EXPECT_TRUE(strToxt != constStrText);
    EXPECT_TRUE(constStrText != strToxt);

    std::string strTex("Tex");
    EXPECT_TRUE(strTex != constStrText);
    EXPECT_TRUE(constStrText != strTex);

    EXPECT_TRUE(ConstStringRef("Text") == "Text");
    EXPECT_TRUE("Text" == ConstStringRef("Text"));
    EXPECT_FALSE(ConstStringRef("Tex") == "Text");
    EXPECT_FALSE("Text" == ConstStringRef("Tex"));
    EXPECT_FALSE(ConstStringRef("Tex") == "Text");
    EXPECT_FALSE("Text" == ConstStringRef("Tex"));

    EXPECT_FALSE(ConstStringRef("Text") != "Text");
    EXPECT_FALSE("Text" != ConstStringRef("Text"));
    EXPECT_TRUE(ConstStringRef("Tex") != "Text");
    EXPECT_TRUE("Text" != ConstStringRef("Tex"));
    EXPECT_TRUE(ConstStringRef("Tex") != "Text");
    EXPECT_TRUE("Text" != ConstStringRef("Tex"));
}

TEST(ConstStringRef, WhenStrIsCalledThenEmitsProperString) {
    static constexpr ConstStringRef constStrText("Text");
    std::string str = constStrText.str();
    EXPECT_EQ(4U, str.size());
    EXPECT_STREQ("Text", str.c_str());
}

TEST(ConstStringRef, WhenDefaultInitializedThenEmpty) {
    ConstStringRef str;
    EXPECT_TRUE(str.empty());
}

TEST(ConstStringRef, WhenCopyConstructedThenIdenticalAsOrigin) {
    static constexpr ConstStringRef a("Text");
    static constexpr ConstStringRef b(a);
    EXPECT_EQ(a, b);
}

TEST(ConstStringRef, WhenCopyAsignedThenIdenticalAsOrigin) {
    static constexpr ConstStringRef a("Text");
    ConstStringRef b("OtherText");
    b = a;
    EXPECT_EQ(a, b);
}

TEST(ConstStringRef, WhenCheckingForInclusionCaseInsensitivelyThenDoesNotReadOutOfBounds) {
    static constexpr ConstStringRef str1("Text", 2);
    ConstStringRef substr1("tex");
    EXPECT_FALSE(str1.containsCaseInsensitive(substr1.data()));

    static constexpr ConstStringRef str2("AabAac");
    ConstStringRef substr2("aac");
    EXPECT_TRUE(str2.containsCaseInsensitive(substr2.data()));

    static constexpr ConstStringRef str3("AabAac");
    ConstStringRef substr3("aacd");
    EXPECT_FALSE(str3.containsCaseInsensitive(substr3.data()));
}

TEST(ConstStringRef, GivenConstStringRefWithDifferentCasesWhenCheckingIfOneContainsTheOtherOneCaseInsensitivelyThenTrueIsReturned) {
    static constexpr ConstStringRef str1("TexT");
    static constexpr ConstStringRef str2("tEXt");

    EXPECT_FALSE(str1.contains(str2.data()));
    EXPECT_TRUE(str1.containsCaseInsensitive(str2.data()));
}

TEST(ConstStringRef, WhenCheckingForInclusionThenDoesNotReadOutOfBounds) {
    static constexpr ConstStringRef str1("Text", 2);
    ConstStringRef substr1("Tex");
    EXPECT_FALSE(str1.contains(substr1.data()));

    static constexpr ConstStringRef str2("AabAac");
    ConstStringRef substr2("Aac");
    EXPECT_TRUE(str2.contains(substr2.data()));

    static constexpr ConstStringRef str3("AabAac");
    ConstStringRef substr3("Aacd");
    EXPECT_FALSE(str3.contains(substr3.data()));
}

TEST(ConstStringRef, WhenCreatingFromStringThenUsesUnderlyingDataAndLength) {
    std::string src = "abc";
    ConstStringRef fromCopy = src;
    EXPECT_EQ(src.data(), fromCopy.begin());
    EXPECT_EQ(src.data() + src.size(), fromCopy.end());
    ConstStringRef fromMove = std::move(src);
    EXPECT_EQ(fromCopy.begin(), fromMove.begin());
    EXPECT_EQ(fromCopy.end(), fromMove.end());
}

TEST(ConstStringRef, WhenCreatingFromCStringThenImplicitlyCalculatesLength) {
    const char *src = "text";
    ConstStringRef fromCString = src;
    EXPECT_EQ(src, fromCString.begin());
    EXPECT_EQ(4U, fromCString.size());
}

TEST(ConstStringRefSubstr, GivenPositiveLengthThenCountFromLeft) {
    ConstStringRef fromCString = "some text";
    ConstStringRef substr = fromCString.substr(2, 2);
    EXPECT_EQ(fromCString.data() + 2, substr.begin());
    EXPECT_EQ(fromCString.data() + 4, substr.end());
    EXPECT_EQ(2U, substr.length());
}

TEST(ConstStringRefSubstr, GivenNegativeLengthThenCountFromRight) {
    ConstStringRef fromCString = "some text";
    ConstStringRef substr = fromCString.substr(2, -2);
    EXPECT_EQ(fromCString.data() + 2, substr.begin());
    EXPECT_EQ(fromCString.data() + fromCString.length() - 2, substr.end());
    EXPECT_EQ(5U, substr.length());
}

TEST(ConstStringRefSubstr, GivenOnlyPositionThenReturnRemainingPartOfString) {
    ConstStringRef fromCString = "some text";
    ConstStringRef substr = fromCString.substr(2);
    EXPECT_EQ(fromCString.data() + 2, substr.begin());
    EXPECT_EQ(fromCString.end(), substr.end());
    EXPECT_EQ(fromCString.length() - 2, substr.length());
}

TEST(ConstStringRefTruncated, GivenPositiveLengthThenCountFromLeft) {
    ConstStringRef fromCString = "some text";
    ConstStringRef substr = fromCString.truncated(2);
    EXPECT_EQ(fromCString.begin(), substr.begin());
    EXPECT_EQ(fromCString.begin() + 2, substr.end());
    EXPECT_EQ(2U, substr.length());
}

TEST(ConstStringRefTruncated, GivenNegativeLengthThenCountFromRight) {
    ConstStringRef fromCString = "some text";
    ConstStringRef substr = fromCString.truncated(-2);
    EXPECT_EQ(fromCString.begin(), substr.begin());
    EXPECT_EQ(fromCString.data() + fromCString.length() - 2, substr.end());
    EXPECT_EQ(7U, substr.length());
}

TEST(ConstStringRefEqualsCaseInsensitive, WhenSizesDifferThenReturnFalse) {
    ConstStringRef lhs = ConstStringRef::fromArray("\0");
    ConstStringRef rhs = ConstStringRef::fromArray("\0\0");
    EXPECT_FALSE(equalsCaseInsensitive(lhs, rhs));
}

TEST(ConstStringRefEqualsCaseInsensitive, WhenStringsDontMatchThenReturnFalse) {
    EXPECT_FALSE(equalsCaseInsensitive(ConstStringRef("abc"), ConstStringRef("abd")));
}

TEST(ConstStringRefEqualsCaseInsensitive, WhenStringsIdenticalThenReturnTrue) {
    EXPECT_TRUE(equalsCaseInsensitive(ConstStringRef("abc"), ConstStringRef("abc")));
}

TEST(ConstStringRefEqualsCaseInsensitive, WhenStringsDifferOnlyByCaseThenReturnTrue) {
    EXPECT_TRUE(equalsCaseInsensitive(ConstStringRef("aBc"), ConstStringRef("Abc")));
}

TEST(ConstStringStartsWith, GivenRightPrefixThenReturnsTrue) {
    ConstStringRef str = "some text";
    EXPECT_TRUE(str.startsWith("some"));
    EXPECT_TRUE(str.startsWith("some text"));
}

TEST(ConstStringStartsWith, GivenInvalidPrefixThenReturnsFalse) {
    ConstStringRef str = "some text";
    EXPECT_FALSE(str.startsWith("ome"));
    EXPECT_FALSE(str.startsWith("some text "));
    EXPECT_FALSE(str.startsWith("substr some text"));
}

TEST(ConstStringStartsWithConstString, GivenRightPrefixThenReturnsTrue) {
    ConstStringRef str = "some text";
    EXPECT_TRUE(str.startsWith(ConstStringRef("some")));
    EXPECT_TRUE(str.startsWith(ConstStringRef("some text")));
}

TEST(ConstStringStartsWithConstString, GivenInvalidPrefixThenReturnsFalse) {
    ConstStringRef str = "some text";
    EXPECT_FALSE(str.startsWith(ConstStringRef("some text and more")));
    EXPECT_FALSE(str.startsWith(ConstStringRef("some diff")));
    EXPECT_FALSE(str.startsWith(ConstStringRef("ome text")));
}

TEST(ConstStringRefTrimEnd, givenTrimEndFunctionWithPredicateThenReturnTrimmedConstStringRefAccordingToPredicate) {
    auto predicateIsNumber = [](char c) {
        return c >= '0' && c <= '9';
    };
    ConstStringRef str = "some 10 text1024";
    auto trimmed = str.trimEnd(predicateIsNumber);
    EXPECT_EQ(trimmed, ConstStringRef("some 10 text"));
}

TEST(ConstStringRefTrimEnd, givenTrimEndFunctionWithPredicateThatReturnsTrueForAllElementsThenReturnEmptyConstStringRef) {
    auto predicate = [](char c) { return true; };
    ConstStringRef str = "some text";
    auto trimmed = str.trimEnd(predicate);
    EXPECT_EQ(0u, trimmed.length());
}

TEST(ConstStringJoin, givenEmptyListOfStringsToJoinThenReturnEmptyString) {
    std::string joined = NEO::ConstStringRef(" ").join(std::vector<std::string>{});
    EXPECT_TRUE(joined.empty());
}

TEST(ConstStringJoin, givenOneStringOnListThenReturnThatString) {
    std::string a = "aa";
    std::string joined = NEO::ConstStringRef(" ").join(std::vector<std::string>{a});
    ASSERT_FALSE(joined.empty());
    EXPECT_EQ(a, joined);
}

TEST(ConstStringJoin, givenTwoStringOnListThenReturnThoseStringsJoinedWithConstring) {
    std::string a = "aa";
    std::string b = "bb";
    std::string joined = NEO::ConstStringRef(" ").join(std::vector<std::string>{a, b});
    ASSERT_FALSE(joined.empty());
    EXPECT_EQ(std::string("aa bb"), joined);
}
