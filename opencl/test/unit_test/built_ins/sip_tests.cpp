/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace SipKernelTests {

TEST(Sip, WhenGettingTypeThenCorrectTypeIsReturned) {
    std::vector<char> ssaHeader;
    SipKernel csr{SipKernelType::Csr, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::Csr, csr.getType());

    SipKernel dbgCsr{SipKernelType::DbgCsr, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::DbgCsr, dbgCsr.getType());

    SipKernel dbgCsrLocal{SipKernelType::DbgCsrLocal, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::DbgCsrLocal, dbgCsrLocal.getType());

    SipKernel undefined{SipKernelType::COUNT, nullptr, ssaHeader};
    EXPECT_EQ(SipKernelType::COUNT, undefined.getType());
}

TEST(Sip, givenDebuggingInactiveWhenSipTypeIsQueriedThenCsrSipTypeIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto sipType = SipKernel::getSipKernelType(*mockDevice);
    EXPECT_EQ(SipKernelType::Csr, sipType);
}

TEST(DebugSip, givenDebuggingActiveWhenSipTypeIsQueriedThenDbgCsrSipTypeIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->setDebuggerActive(true);

    auto sipType = SipKernel::getSipKernelType(*mockDevice);
    EXPECT_LE(SipKernelType::DbgCsr, sipType);
}

TEST(DebugSip, givenBuiltInsWhenDbgCsrSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto &builtins = *mockDevice->getBuiltIns();
    auto &sipKernel = builtins.getSipKernel(SipKernelType::DbgCsr, *mockDevice);

    EXPECT_NE(nullptr, &sipKernel);
    EXPECT_EQ(SipKernelType::DbgCsr, sipKernel.getType());
}

TEST(DebugBindlessSip, givenBindlessDebugSipIsRequestedThenCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);

    auto &sipKernel = NEO::SipKernel::getBindlessDebugSipKernel(*mockDevice);

    EXPECT_NE(nullptr, &sipKernel);
    EXPECT_EQ(SipKernelType::DbgBindless, sipKernel.getType());

    EXPECT_FALSE(sipKernel.getStateSaveAreaHeader().empty());
}
} // namespace SipKernelTests
