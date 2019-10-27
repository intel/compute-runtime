/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "runtime/program/kernel_info.h"
#include "runtime/program/kernel_info_from_patchtokens.h"
#include "unit_tests/compiler_interface/patchtokens_tests.h"

TEST(GetInlineData, GivenValidEmptyKernelFromPatchtokensThenReturnEmptyKernelInfo) {
    std::vector<uint8_t> storage;
    auto src = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::KernelInfo dst = {};
    NEO::populateKernelInfo(dst, src);

    NEO::KernelInfo expectedKernelInfo = {};
    expectedKernelInfo.name = std::string(src.name.begin()).c_str();
    expectedKernelInfo.heapInfo.pKernelHeader = src.header;
    expectedKernelInfo.isValid = true;

    EXPECT_STREQ(expectedKernelInfo.name.c_str(), dst.name.c_str());
    EXPECT_EQ(expectedKernelInfo.heapInfo.pKernelHeader, dst.heapInfo.pKernelHeader);
    EXPECT_EQ(expectedKernelInfo.isValid, dst.isValid);
}
