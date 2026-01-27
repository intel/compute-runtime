/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/program/kernel_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(AddressingModeHelperTest, GivenArgIsNotPointerWhenCheckingForBufferStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTValue);

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsBufferStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenArgIsPointerWithInvalidStatefulOffsetWhenCheckingForBufferStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argDescriptor.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsBufferStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenArgIsPointerWithValidBindfulOffsetWhenCheckingForBufferStatefulAccessThenReturnTrue) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = 0x40;
    argDescriptor.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_TRUE(AddressingModeHelper::containsBufferStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenArgIsPointerWithValidBindlessOffsetWhenCheckingForBufferStatefulAccessThenReturnTrue) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argDescriptor.as<ArgDescPointer>().bindless = 0x40;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_TRUE(AddressingModeHelper::containsBufferStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenLastArgIsPointerWithValidBindlessOffsetWhenIgnoreLastArgAndCheckingForBufferStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argDescriptor.as<ArgDescPointer>().bindless = 0x40;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsBufferStatefulAccess(kernelDescriptor, true));
}

TEST(AddressingModeHelperTest, GivenInvalidArgWhenCheckingForStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTUnknown);

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));
}

TEST(AddressingModeHelperTest, GivenValidArgWithInvalidStatefulOffsetWhenCheckingForStatefulAccessThenReturnFalse) {
    auto argTPointer = ArgDescriptor(ArgDescriptor::argTPointer);
    argTPointer.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argTPointer.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;

    auto argTImage = ArgDescriptor(ArgDescriptor::argTImage);
    argTImage.as<ArgDescImage>().bindful = undefined<SurfaceStateHeapOffset>;
    argTImage.as<ArgDescImage>().bindless = undefined<CrossThreadDataOffset>;

    auto argTSampler = ArgDescriptor(ArgDescriptor::argTSampler);
    argTSampler.as<ArgDescSampler>().bindful = undefined<SurfaceStateHeapOffset>;
    argTSampler.as<ArgDescSampler>().bindless = undefined<CrossThreadDataOffset>;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTPointer);
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTImage);
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTSampler);

    EXPECT_FALSE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));
}

TEST(AddressingModeHelperTest, GivenValidArgWithValidBindfulOffsetWhenCheckingForStatefulAccessThenReturnTrue) {
    auto argTPointer = ArgDescriptor(ArgDescriptor::argTPointer);
    argTPointer.as<ArgDescPointer>().bindful = 0x40;
    argTPointer.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTPointer);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));

    auto argTImage = ArgDescriptor(ArgDescriptor::argTImage);
    argTImage.as<ArgDescImage>().bindful = 0x40;
    argTImage.as<ArgDescImage>().bindless = undefined<CrossThreadDataOffset>;

    kernelDescriptor.payloadMappings.explicitArgs.clear();
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTImage);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));

    auto argTSampler = ArgDescriptor(ArgDescriptor::argTSampler);
    argTSampler.as<ArgDescSampler>().bindful = 0x40;
    argTSampler.as<ArgDescSampler>().bindless = undefined<CrossThreadDataOffset>;

    kernelDescriptor.payloadMappings.explicitArgs.clear();
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTSampler);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));
}

TEST(AddressingModeHelperTest, GivenValidArgWithValidBindlessOffsetWhenCheckingForStatefulAccessThenReturnTrue) {
    auto argTPointer = ArgDescriptor(ArgDescriptor::argTPointer);
    argTPointer.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argTPointer.as<ArgDescPointer>().bindless = 0x40;

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTPointer);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));

    auto argTImage = ArgDescriptor(ArgDescriptor::argTImage);
    argTImage.as<ArgDescImage>().bindful = undefined<SurfaceStateHeapOffset>;
    argTImage.as<ArgDescImage>().bindless = 0x40;

    kernelDescriptor.payloadMappings.explicitArgs.clear();
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTImage);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));

    auto argTSampler = ArgDescriptor(ArgDescriptor::argTSampler);
    argTSampler.as<ArgDescSampler>().bindful = undefined<SurfaceStateHeapOffset>;
    argTSampler.as<ArgDescSampler>().bindless = 0x40;

    kernelDescriptor.payloadMappings.explicitArgs.clear();
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argTSampler);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor));
}

TEST(AddressingModeHelperTest, GivenKernelInfosWhenCheckingForBindlessKernelThenReturnCorrectValue) {
    KernelInfo kernelInfo1{};
    KernelInfo kernelInfo2{};

    {
        kernelInfo1.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindless;
        std::vector<KernelInfo *> kernelInfos{&kernelInfo1};
        EXPECT_TRUE(AddressingModeHelper::containsBindlessKernel(kernelInfos));
    }

    {
        kernelInfo1.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindful;
        std::vector<KernelInfo *> kernelInfos{&kernelInfo1};
        EXPECT_FALSE(AddressingModeHelper::containsBindlessKernel(kernelInfos));
    }

    {
        kernelInfo1.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindful;
        std::vector<KernelInfo *> kernelInfos{&kernelInfo1};
        EXPECT_FALSE(AddressingModeHelper::containsBindlessKernel(kernelInfos));
    }

    {
        kernelInfo1.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindful;
        kernelInfo2.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindless;
        std::vector<KernelInfo *> kernelInfos{&kernelInfo1, &kernelInfo2};
        EXPECT_TRUE(AddressingModeHelper::containsBindlessKernel(kernelInfos));
    }
}

TEST(AddressingModeHelperTest, GivenSingleValueWithinU32RangeThenReturnsFalse) {
    constexpr auto u32Max = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());

    EXPECT_FALSE(AddressingModeHelper::isAnyValueWiderThan32bit(uint64_t{0}));
    EXPECT_FALSE(AddressingModeHelper::isAnyValueWiderThan32bit(uint64_t{1}));
    EXPECT_FALSE(AddressingModeHelper::isAnyValueWiderThan32bit(u32Max));
}

TEST(AddressingModeHelperTest, GivenSingleValueAboveU32MaxThenReturnsTrue) {
    constexpr auto u32Max = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    constexpr auto above = u32Max + 1ULL;

    EXPECT_TRUE(AddressingModeHelper::isAnyValueWiderThan32bit(uint64_t{above}));
    EXPECT_TRUE(AddressingModeHelper::isAnyValueWiderThan32bit(std::numeric_limits<uint64_t>::max()));
}

TEST(AddressingModeHelperTest, GivenAllArgsWithinU32RangeThenReturnsFalse) {
    constexpr auto u32Max = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());

    EXPECT_FALSE(AddressingModeHelper::isAnyValueWiderThan32bit(uint64_t{0}, uint64_t{123}, uint64_t{u32Max}));
}

TEST(AddressingModeHelperTest, GivenAnyArgAboveU32MaxThenReturnsTrue) {
    constexpr auto u32Max = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    constexpr auto above = u32Max + 1ULL;

    EXPECT_TRUE(AddressingModeHelper::isAnyValueWiderThan32bit(uint64_t{0}, uint64_t{u32Max}, uint64_t{above}));
    EXPECT_TRUE(AddressingModeHelper::isAnyValueWiderThan32bit(uint64_t{above}, uint64_t{0}));
}