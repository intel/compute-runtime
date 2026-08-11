/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/test/common/mocks/mock_compiler_release_helper.h"

#include "gtest/gtest.h"

using namespace NEO;
using CompilerReleaseHelperKernelCapabilitiesTests = ::testing::Test;

TEST(CompilerReleaseHelperKernelCapabilitiesTests, givenNoAdditionalFp16CapsWhenGettingKernelFp16AtomicCapabilitiesThenReturnMinMaxAndLoadStoreCapabilities) {
    MockCompilerReleaseHelper compilerReleaseHelper;
    compilerReleaseHelper.getAdditionalFp16CapsResult = 0u;

    uint32_t fp16Caps = 0u;
    compilerReleaseHelper.getKernelFp16AtomicCapabilities(fp16Caps);

    EXPECT_EQ(1u, compilerReleaseHelper.getAdditionalFp16CapsCalled);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, fp16Caps);
}

TEST(CompilerReleaseHelperKernelCapabilitiesTests, givenAdditionalFp16CapsWhenGettingKernelFp16AtomicCapabilitiesThenAdditionalCapsAreOredIn) {
    MockCompilerReleaseHelper compilerReleaseHelper;
    compilerReleaseHelper.getAdditionalFp16CapsResult = FpAtomicExtFlags::addAtomicCaps;

    uint32_t fp16Caps = 0u;
    compilerReleaseHelper.getKernelFp16AtomicCapabilities(fp16Caps);

    EXPECT_EQ(1u, compilerReleaseHelper.getAdditionalFp16CapsCalled);
    EXPECT_EQ(FpAtomicExtFlags::addAtomicCaps | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, fp16Caps);
}
