/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "test.h"

TEST(HasSameMagic, WhenMagicIsMatchedThenReturnTrue) {
    EXPECT_TRUE(NEO::hasSameMagic("abcd", ArrayRef<const char>("abcdefg").toArrayRef<const uint8_t>()));
}

TEST(HasSameMagic, WhenBinaryIsNullptrThenReturnFalse) {
    EXPECT_FALSE(NEO::hasSameMagic("abcd", {}));
}

TEST(HasSameMagic, WhenBinaryIsShorterThanExpectedMagicThenReturnFalse) {
    EXPECT_FALSE(NEO::hasSameMagic("abcd", ArrayRef<const char>("ab").toArrayRef<const uint8_t>()));
}

TEST(HasSameMagic, WhenMagicIsNotMatchedThenReturnFalse) {
    EXPECT_FALSE(NEO::hasSameMagic("abcd", ArrayRef<const char>("abcefg").toArrayRef<const uint8_t>()));
}

static constexpr uint8_t llvmBinary[] = "BC\xc0\xde     ";

TEST(IsLlvmBitcode, WhenLlvmMagicWasFoundThenBinaryIsValidLLvm) {
    EXPECT_TRUE(NEO::isLlvmBitcode(llvmBinary));
}

TEST(IsLlvmBitcode, WhenBinaryIsNullptrThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(NEO::isLlvmBitcode(ArrayRef<const uint8_t>()));
}

TEST(IsLlvmBitcode, WhenBinaryIsShorterThanLllvMagicThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(NEO::isLlvmBitcode(ArrayRef<const uint8_t>(llvmBinary, 2)));
}

TEST(IsLlvmBitcode, WhenBinaryDoesNotContainLllvMagicThenBinaryIsNotValidLLvm) {
    const uint8_t notLlvmBinary[] = "ABCDEFGHIJKLMNO";
    EXPECT_FALSE(NEO::isLlvmBitcode(notLlvmBinary));
}

static constexpr uint32_t spirv[16] = {0x03022307};
static constexpr uint32_t spirvInvEndianes[16] = {0x07230203};

TEST(IsSpirVBitcode, WhenSpirvMagicWasFoundThenBinaryIsValidSpirv) {
    EXPECT_TRUE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(&spirv), sizeof(spirv))));
    EXPECT_TRUE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(&spirvInvEndianes), sizeof(spirvInvEndianes))));
}

TEST(IsSpirVBitcode, WhenBinaryIsNullptrThenBinaryIsNotValidSpirv) {
    EXPECT_FALSE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>()));
}

TEST(IsSpirVBitcode, WhenBinaryIsShorterThanLllvMagicThenBinaryIsNotValidSpirv) {
    EXPECT_FALSE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(&spirvInvEndianes), 2)));
}

TEST(IsSpirVBitcode, WhenBinaryDoesNotContainLllvMagicThenBinaryIsNotValidSpirv) {
    const uint8_t notSpirvBinary[] = "ABCDEFGHIJKLMNO";
    EXPECT_FALSE(NEO::isSpirVBitcode(notSpirvBinary));
}
