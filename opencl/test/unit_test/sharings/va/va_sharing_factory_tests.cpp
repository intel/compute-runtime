/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/va/enable_va.h"

#include "gtest/gtest.h"

using namespace NEO;

class TestVaSharingBuilderFactory : public VaSharingBuilderFactory {
  public:
    void *getExtensionFunctionAddressExtra(const std::string &functionName) override {
        if (functionName == "someFunction") {
            invocationCount++;
            return reinterpret_cast<void *>(0x1234);
        }
        return nullptr;
    }
    uint32_t invocationCount = 0u;
};

TEST(SharingFactoryTests, givenVaFactoryWithSharingExtraWhenAskedThenAddressIsReturned) {
    TestVaSharingBuilderFactory sharing;

    ASSERT_EQ(0u, sharing.invocationCount);

    auto ptr = sharing.getExtensionFunctionAddress("someFunction");
    EXPECT_NE(nullptr, ptr);

    EXPECT_EQ(1u, sharing.invocationCount);
}