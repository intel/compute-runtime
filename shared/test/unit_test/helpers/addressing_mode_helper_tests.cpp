/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/program/kernel_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(AddressingModeHelperTest, GivenArgIsNotPointerWhenCheckingForStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTValue);

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenArgIsPointerWithInvalidStatefulOffsetWhenCheckingForStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argDescriptor.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenArgIsPointerWithValidBindfulOffsetWhenCheckingForStatefulAccessThenReturnTrue) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = 0x40;
    argDescriptor.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenArgIsPointerWithValidBindlessOffsetWhenCheckingForStatefulAccessThenReturnTrue) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argDescriptor.as<ArgDescPointer>().bindless = 0x40;

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_TRUE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor, false));
}

TEST(AddressingModeHelperTest, GivenLastArgIsPointerWithValidBindlessOffsetWhenIgnoreLastArgAndCheckingForStatefulAccessThenReturnFalse) {
    auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
    argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
    argDescriptor.as<ArgDescPointer>().bindless = 0x40;

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    EXPECT_FALSE(AddressingModeHelper::containsStatefulAccess(kernelDescriptor, true));
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
