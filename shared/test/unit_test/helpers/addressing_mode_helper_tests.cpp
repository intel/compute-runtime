/*
 * Copyright (C) 2023-2025 Intel Corporation
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
