/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gtest/gtest.h"
#include "test.h"

#include "runtime/built_ins/sip.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_device_factory.h"

using namespace OCLRT;

TEST(Sip, WhenSipKernelIsInvalidThenEmptyCompilerInternalOptionsAreReturned) {
    const char *opt = getSipKernelCompilerInternalOptions(SipKernelType::COUNT);
    ASSERT_NE(nullptr, opt);
    EXPECT_EQ(0U, strlen(opt));
}

TEST(Sip, WhenRequestingCsrSipKernelThenProperCompilerInternalOptionsAreReturned) {
    const char *opt = getSipKernelCompilerInternalOptions(SipKernelType::Csr);
    ASSERT_NE(nullptr, opt);
    EXPECT_STREQ("-cl-include-sip-csr", opt);
}

TEST(Sip, When32BitAddressesAreNotBeingForcedThenSipLlHasSameBitnessAsHostApplication) {
    auto mockDevice = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->setForce32BitAddressing(false);
    const char *src = getSipLlSrc(*mockDevice);
    ASSERT_NE(nullptr, src);
    if (sizeof(void *) == 8) {
        EXPECT_NE(nullptr, strstr(src, "target datalayout = \"e-p:64:64:64\""));
        EXPECT_NE(nullptr, strstr(src, "target triple = \"spir64\""));
    } else {
        EXPECT_NE(nullptr, strstr(src, "target datalayout = \"e-p:32:32:32\""));
        EXPECT_NE(nullptr, strstr(src, "target triple = \"spir\""));
        EXPECT_EQ(nullptr, strstr(src, "target triple = \"spir64\""));
    }
}

TEST(Sip, When32BitAddressesAreBeingForcedThenSipLlHas32BitAddresses) {
    auto mockDevice = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->setForce32BitAddressing(true);
    const char *src = getSipLlSrc(*mockDevice);
    ASSERT_NE(nullptr, src);
    EXPECT_NE(nullptr, strstr(src, "target datalayout = \"e-p:32:32:32\""));
    EXPECT_NE(nullptr, strstr(src, "target triple = \"spir\""));
    EXPECT_EQ(nullptr, strstr(src, "target triple = \"spir64\""));
}

TEST(Sip, SipLlContainsMetadataRequiredByCompiler) {
    auto mockDevice = std::unique_ptr<MockDevice>(Device::create<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    const char *src = getSipLlSrc(*mockDevice);
    ASSERT_NE(nullptr, src);

    EXPECT_NE(nullptr, strstr(src, "!opencl.compiler.options"));
    EXPECT_NE(nullptr, strstr(src, "!opencl.kernels"));
}
