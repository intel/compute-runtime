/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/utilities/const_stringref.h"

#include "gtest/gtest.h"

TEST(ConstStringRef, WhenCreatingFromConstantArrayThenIsConstexpr) {
    static constexpr ConstStringRef str0("some_text");
    static_assert(9 == str0.length(), "");
    static_assert(9 == str0.size(), "");
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

    static_assert('s' == static_cast<const char *>(str1)[0], "");
    static_assert('s' == *str1.begin(), "");
    static_assert(3 == str1.end() - str1.begin(), "");
    static_assert(str1.begin() == str1.data(), "");

    static constexpr ConstStringRef strEmpty("aaa", 0);
    static_assert(0U == strEmpty.length(), "");
    static_assert(0U == strEmpty.size(), "");
    static_assert(strEmpty.empty(), "");

    static_assert(9 == str0.length(), "");

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

TEST(ConstStringRef, WhenCheckingForInclusionThenDoesNotReadOutOfBounds) {
    static constexpr ConstStringRef str1("Text", 2);
    ConstStringRef substr1("Tex");
    EXPECT_FALSE(str1.contains(substr1));

    static constexpr ConstStringRef str2("AabAac");
    ConstStringRef substr2("Aac");
    EXPECT_TRUE(str2.contains(substr2));

    static constexpr ConstStringRef str3("AabAac");
    ConstStringRef substr3("Aacd");
    EXPECT_FALSE(str3.contains(substr3));
}
