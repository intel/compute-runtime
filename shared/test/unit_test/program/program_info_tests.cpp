/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"

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

TEST(RequiresRebuildWithPatchtokens, givenLegacyDebuggerAttachedAndZebinaryFormatNonBuiltinBinaryWhenCheckingForRebuildRequirementThenReturnTrueAndFallbackToPatchtokens) {
    ZebinTestData::ValidEmptyProgram<> zebin;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->getRootDeviceEnvironmentRef().debugger.reset(new MockActiveSourceLevelDebugger);
    std::string options{NEO::CompilerOptions::allowZebin};
    bool isBuiltIn{false};

    bool rebuildRequired = isRebuiltToPatchtokensRequired(device.get(), ArrayRef<const uint8_t>::fromAny(zebin.storage.data(), zebin.storage.size()), options, isBuiltIn);
    EXPECT_TRUE(rebuildRequired);
    EXPECT_FALSE(NEO::CompilerOptions::contains(options, NEO::CompilerOptions::allowZebin));
    EXPECT_TRUE(NEO::CompilerOptions::contains(options, NEO::CompilerOptions::disableZebin));
}

TEST(RequiresRebuildWithPatchtokens, givenNoLegacyDebuggerAttachedOrNonZebinaryFormatOrBuiltinBinaryWhenCheckingForRebuildRequirementThenReturnFalseAndDoNotFallback) {
    ZebinTestData::ValidEmptyProgram<> zebin;
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    device->getRootDeviceEnvironmentRef().debugger.reset(new MockActiveSourceLevelDebugger);
    std::string options{NEO::CompilerOptions::allowZebin};
    bool isBuiltIn{true};

    bool rebuildRequired = isRebuiltToPatchtokensRequired(device.get(), ArrayRef<const uint8_t>::fromAny(zebin.storage.data(), zebin.storage.size()), options, isBuiltIn);
    EXPECT_FALSE(rebuildRequired);
    EXPECT_TRUE(NEO::CompilerOptions::contains(options, NEO::CompilerOptions::allowZebin));

    isBuiltIn = false;
    device->getRootDeviceEnvironmentRef().debugger.reset(nullptr);
    rebuildRequired = isRebuiltToPatchtokensRequired(device.get(), ArrayRef<const uint8_t>::fromAny(zebin.storage.data(), zebin.storage.size()), options, isBuiltIn);
    EXPECT_FALSE(rebuildRequired);
    EXPECT_TRUE(NEO::CompilerOptions::contains(options, NEO::CompilerOptions::allowZebin));

    device->getRootDeviceEnvironmentRef().debugger.reset(new MockActiveSourceLevelDebugger);
    for (auto idx = 0; idx < 4; idx++) {
        zebin.elfHeader->identity.magic[idx] = 0;
    } //broken header - zebinary format will not be detected
    rebuildRequired = isRebuiltToPatchtokensRequired(device.get(), ArrayRef<const uint8_t>::fromAny(zebin.storage.data(), zebin.storage.size()), options, isBuiltIn);
    EXPECT_FALSE(rebuildRequired);
    EXPECT_TRUE(NEO::CompilerOptions::contains(options, NEO::CompilerOptions::allowZebin));
}