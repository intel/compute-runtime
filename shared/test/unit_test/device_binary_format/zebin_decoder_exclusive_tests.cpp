/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zeinfo_decoder_ext.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO::Zebin;

TEST(ExtBaseKernelDescriptorAndPayloadArgumentPointers, givenKernelDescriptorAndPayloadArgWhenProperlyCreatedThenExtraFieldsSetToNullPtrValue) {
    NEO::KernelDescriptor kd;
    NEO::Zebin::ZeInfo::KernelPayloadArgBaseT arg;

    EXPECT_EQ(nullptr, kd.kernelDescriptorExt);
    EXPECT_EQ(nullptr, arg.pPayArgExt);
    EXPECT_EQ(nullptr, NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::allocatePayloadArgumentExt());
}
