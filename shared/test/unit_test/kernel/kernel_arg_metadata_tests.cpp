/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_arg_metadata.h"

#include "test.h"

TEST(KernelArgMetadata, WhenParseAccessQualifierIsCalledThenQualifierIsProperlyParsed) {
    using namespace NEO;
    using namespace NEO::KernelArgMetadata;
    EXPECT_EQ(AccessNone, KernelArgMetadata::parseAccessQualifier(""));
    EXPECT_EQ(AccessNone, KernelArgMetadata::parseAccessQualifier("NONE"));
    EXPECT_EQ(AccessReadOnly, KernelArgMetadata::parseAccessQualifier("read_only"));
    EXPECT_EQ(AccessWriteOnly, KernelArgMetadata::parseAccessQualifier("write_only"));
    EXPECT_EQ(AccessReadWrite, KernelArgMetadata::parseAccessQualifier("read_write"));
    EXPECT_EQ(AccessReadOnly, KernelArgMetadata::parseAccessQualifier("__read_only"));
    EXPECT_EQ(AccessWriteOnly, KernelArgMetadata::parseAccessQualifier("__write_only"));
    EXPECT_EQ(AccessReadWrite, KernelArgMetadata::parseAccessQualifier("__read_write"));

    EXPECT_EQ(AccessUnknown, KernelArgMetadata::parseAccessQualifier("re"));
    EXPECT_EQ(AccessUnknown, KernelArgMetadata::parseAccessQualifier("read"));
    EXPECT_EQ(AccessUnknown, KernelArgMetadata::parseAccessQualifier("write"));
}

TEST(KernelArgMetadata, WhenParseAddressQualifierIsCalledThenQualifierIsProperlyParsed) {
    using namespace NEO;
    using namespace NEO::KernelArgMetadata;
    EXPECT_EQ(AddrGlobal, KernelArgMetadata::parseAddressSpace(""));
    EXPECT_EQ(AddrGlobal, KernelArgMetadata::parseAddressSpace("__global"));
    EXPECT_EQ(AddrLocal, KernelArgMetadata::parseAddressSpace("__local"));
    EXPECT_EQ(AddrPrivate, KernelArgMetadata::parseAddressSpace("__private"));
    EXPECT_EQ(AddrConstant, KernelArgMetadata::parseAddressSpace("__constant"));
    EXPECT_EQ(AddrPrivate, KernelArgMetadata::parseAddressSpace("not_specified"));

    EXPECT_EQ(AddrUnknown, KernelArgMetadata::parseAddressSpace("wrong"));
    EXPECT_EQ(AddrUnknown, KernelArgMetadata::parseAddressSpace("__glob"));
    EXPECT_EQ(AddrUnknown, KernelArgMetadata::parseAddressSpace("__loc"));
    EXPECT_EQ(AddrUnknown, KernelArgMetadata::parseAddressSpace("__priv"));
    EXPECT_EQ(AddrUnknown, KernelArgMetadata::parseAddressSpace("__const"));
    EXPECT_EQ(AddrUnknown, KernelArgMetadata::parseAddressSpace("not"));
}

TEST(KernelArgMetadata, WhenParseTypeQualifiersIsCalledThenQualifierIsProperlyParsed) {
    using namespace NEO;
    using namespace NEO::KernelArgMetadata;

    TypeQualifiers qual = {};
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("").packed);

    qual = {};
    qual.constQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("const").packed);

    qual = {};
    qual.volatileQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("volatile").packed);

    qual = {};
    qual.restrictQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("restrict").packed);

    qual = {};
    qual.pipeQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pipe").packed);

    qual = {};
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("inval").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("cons").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("volat").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("restr").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pip").packed);

    qual = {};
    qual.constQual = true;
    qual.volatileQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("const volatile").packed);

    qual = {};
    qual.constQual = true;
    qual.volatileQual = true;
    qual.restrictQual = true;
    qual.pipeQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pipe const restrict volatile").packed);

    qual = {};
    qual.constQual = true;
    qual.volatileQual = true;
    qual.restrictQual = true;
    qual.pipeQual = true;
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pipe const restrict volatile some").packed);
}

TEST(KernelArgMetadata, WhenParseLimitedStringIsCalledThenReturnedStringDoesntContainExcessiveTrailingZeroes) {
    char str1[] = "abcd\0\0\0after\0";
    EXPECT_STREQ("abcd", NEO::parseLimitedString(str1, sizeof(str1)).c_str());
    EXPECT_EQ(4U, NEO::parseLimitedString(str1, sizeof(str1)).size());

    EXPECT_STREQ("ab", NEO::parseLimitedString(str1, 2).c_str());
    EXPECT_EQ(2U, NEO::parseLimitedString(str1, 2).size());

    char str2[] = {'a', 'b', 'd', 'e', 'f'};
    EXPECT_STREQ("abdef", NEO::parseLimitedString(str2, sizeof(str2)).c_str());
    EXPECT_EQ(5U, NEO::parseLimitedString(str2, sizeof(str2)).size());
}

TEST(TypeQualifiers, WhenDefaultInitialiedThenFlagsAreCleared) {
    NEO::KernelArgMetadata::TypeQualifiers qual;
    EXPECT_EQ(0U, qual.packed);
}

TEST(TypeQualifiersEmpty, WhenQueriedThenReturnsTrueIfCleared) {
    NEO::KernelArgMetadata::TypeQualifiers qual;
    qual.packed = 0U;
    EXPECT_TRUE(qual.empty());

    qual.constQual = true;
    EXPECT_FALSE(qual.empty());
}

TEST(ArgTypeTraits, WhenDefaultInitialiedThenValuesAreClearedAndAddressSpaceIsGlobal) {
    NEO::ArgTypeTraits argTraits;
    EXPECT_EQ(0U, argTraits.argByValSize);
    EXPECT_EQ(NEO::KernelArgMetadata::AccessUnknown, argTraits.getAccessQualifier());
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, argTraits.getAddressQualifier());
    EXPECT_TRUE(argTraits.typeQualifiers.empty());
}
