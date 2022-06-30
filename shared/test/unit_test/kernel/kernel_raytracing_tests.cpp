/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"

TEST(KernelDescriptorFromPatchtokens, GivenKernelWithRTDispatchGlobalsThenKernelDescriptorIsProperlyPopulated) {
    std::vector<uint8_t> storage;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::KernelDescriptor dst{};
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);

    EXPECT_EQ(0, dst.payloadMappings.implicitArgs.rtDispatchGlobals.stateless);
    EXPECT_EQ(0, dst.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize);
    EXPECT_EQ(0, dst.payloadMappings.implicitArgs.rtDispatchGlobals.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.rtDispatchGlobals.bindless));

    EXPECT_EQ(NEO::KernelDescriptor::BindfulAndStateless, dst.kernelAttributes.bufferAddressingMode);

    iOpenCL::SPatchAllocateRTGlobalBuffer rtDispatchArg = {};
    rtDispatchArg.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_RT_GLOBAL_BUFFER;
    rtDispatchArg.SurfaceStateHeapOffset = 1;
    rtDispatchArg.DataParamOffset = 2;
    rtDispatchArg.DataParamSize = 3;

    kernelTokens.tokens.allocateRTGlobalBuffer = &rtDispatchArg;
    NEO::populateKernelDescriptor(dst, kernelTokens, 4);

    EXPECT_EQ(2, dst.payloadMappings.implicitArgs.rtDispatchGlobals.stateless);
    EXPECT_EQ(3, dst.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize);
    EXPECT_EQ(1, dst.payloadMappings.implicitArgs.rtDispatchGlobals.bindful);
    EXPECT_TRUE(NEO::isUndefinedOffset(dst.payloadMappings.implicitArgs.rtDispatchGlobals.bindless));
}
