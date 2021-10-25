/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(ProgramInfoTests, WhenPrepareLinkerInputStorageGetsCalledTwiceThenLinkerInputStorageIsReused) {
    NEO::ProgramInfo programInfo;
    EXPECT_EQ(nullptr, programInfo.linkerInput);
    programInfo.prepareLinkerInputStorage();
    EXPECT_NE(nullptr, programInfo.linkerInput);
    auto prevLinkerInput = programInfo.linkerInput.get();
    programInfo.prepareLinkerInputStorage();
    EXPECT_EQ(prevLinkerInput, programInfo.linkerInput.get());
}

TEST(GetMaxInlineSlmNeeded, GivenProgramWithoutKernelsThenReturn0) {
    NEO::ProgramInfo programInfo;
    EXPECT_EQ(0U, NEO::getMaxInlineSlmNeeded(programInfo));
}

TEST(GetMaxInlineSlmNeeded, GivenProgramWithKernelsNotRequirignSlmThenReturn0) {
    NEO::ProgramInfo programInfo;
    programInfo.kernelInfos = {new NEO::KernelInfo(), new NEO::KernelInfo(), new NEO::KernelInfo()};
    EXPECT_EQ(0U, NEO::getMaxInlineSlmNeeded(programInfo));
}

TEST(GetMaxInlineSlmNeeded, GivenProgramWithKernelsThenReturnMaxOfInlineSlmNeededByKernels) {
    NEO::ProgramInfo programInfo;
    programInfo.kernelInfos = {new NEO::KernelInfo(), new NEO::KernelInfo(), new NEO::KernelInfo()};
    programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.slmInlineSize = 16;
    programInfo.kernelInfos[1]->kernelDescriptor.kernelAttributes.slmInlineSize = 64;
    programInfo.kernelInfos[2]->kernelDescriptor.kernelAttributes.slmInlineSize = 32;
    EXPECT_EQ(64U, NEO::getMaxInlineSlmNeeded(programInfo));
}

TEST(RequiresLocalMemoryWindowVA, GivenProgramWithoutKernelsThenReturnFalse) {
    NEO::ProgramInfo programInfo;
    EXPECT_FALSE(NEO::requiresLocalMemoryWindowVA(programInfo));
}

TEST(RequiresLocalMemoryWindowVA, GivenProgramWithKernelsNotLocalMemoryWindowVAThenReturnFalse) {
    NEO::ProgramInfo programInfo;
    programInfo.kernelInfos = {new NEO::KernelInfo(), new NEO::KernelInfo(), new NEO::KernelInfo()};
    EXPECT_FALSE(NEO::requiresLocalMemoryWindowVA(programInfo));
}

TEST(RequiresLocalMemoryWindowVA, GivenProgramWithKernelsWhenSomeOfKernelRequireLocalMemoryWidnowVAThenReturnTrue) {
    NEO::ProgramInfo programInfo;
    programInfo.kernelInfos = {new NEO::KernelInfo(), new NEO::KernelInfo(), new NEO::KernelInfo()};
    programInfo.kernelInfos[1]->kernelDescriptor.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres = 0U;
    EXPECT_TRUE(NEO::requiresLocalMemoryWindowVA(programInfo));
}
