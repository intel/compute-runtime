/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/helpers/options.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_device_factory.h"
#include "unit_tests/mocks/mock_program.h"

using namespace OCLRT;

namespace SipKernelTests {
std::string getDebugSipKernelNameWithBitnessAndProductSuffix(std::string &base, const char *product) {
    std::string fullName = base + std::string("_");

    if (sizeof(uintptr_t) == 8) {
        fullName.append("64_");
    } else {
        fullName.append("32_");
    }

    fullName.append(product);
    return fullName;
}

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
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    mockDevice->setForce32BitAddressing(true);
    const char *src = getSipLlSrc(*mockDevice);
    ASSERT_NE(nullptr, src);
    EXPECT_NE(nullptr, strstr(src, "target datalayout = \"e-p:32:32:32\""));
    EXPECT_NE(nullptr, strstr(src, "target triple = \"spir\""));
    EXPECT_EQ(nullptr, strstr(src, "target triple = \"spir64\""));
}

TEST(Sip, SipLlContainsMetadataRequiredByCompiler) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    const char *src = getSipLlSrc(*mockDevice);
    ASSERT_NE(nullptr, src);

    EXPECT_NE(nullptr, strstr(src, "!opencl.compiler.options"));
    EXPECT_NE(nullptr, strstr(src, "!opencl.kernels"));
}

TEST(Sip, getType) {
    SipKernel csr{SipKernelType::Csr, GlobalMockSipProgram::getSipProgramWithCustomBinary()};
    EXPECT_EQ(SipKernelType::Csr, csr.getType());

    SipKernel dbgCsr{SipKernelType::DbgCsr, GlobalMockSipProgram::getSipProgramWithCustomBinary()};
    EXPECT_EQ(SipKernelType::DbgCsr, dbgCsr.getType());

    SipKernel dbgCsrLocal{SipKernelType::DbgCsrLocal, GlobalMockSipProgram::getSipProgramWithCustomBinary()};
    EXPECT_EQ(SipKernelType::DbgCsrLocal, dbgCsrLocal.getType());

    SipKernel undefined{SipKernelType::COUNT, GlobalMockSipProgram::getSipProgramWithCustomBinary()};
    EXPECT_EQ(SipKernelType::COUNT, undefined.getType());
}

TEST(Sip, givenSipKernelClassWhenAskedForMaxDebugSurfaceSizeThenCorrectValueIsReturned) {
    EXPECT_EQ(0x49c000u, SipKernel::maxDbgSurfaceSize);
}

TEST(Sip, givenDebuggingInactiveWhenSipTypeIsQueriedThenCsrSipTypeIsReturned) {
    auto sipType = SipKernel::getSipKernelType(renderCoreFamily, false);
    EXPECT_EQ(SipKernelType::Csr, sipType);
}

TEST(DebugSip, givenDebuggingActiveWhenSipTypeIsQueriedThenDbgCsrSipTypeIsReturned) {
    auto sipType = SipKernel::getSipKernelType(renderCoreFamily, true);
    EXPECT_LE(SipKernelType::DbgCsr, sipType);
}

TEST(DebugSip, WhenRequestingDbgCsrSipKernelThenProperCompilerInternalOptionsAreReturned) {
    const char *opt = getSipKernelCompilerInternalOptions(SipKernelType::DbgCsr);
    ASSERT_NE(nullptr, opt);
    EXPECT_STREQ("-cl-include-sip-kernel-debug -cl-include-sip-csr -cl-set-bti:0", opt);
}

TEST(DebugSip, WhenRequestingDbgCsrWithLocalMemorySipKernelThenProperCompilerInternalOptionsAreReturned) {
    const char *opt = getSipKernelCompilerInternalOptions(SipKernelType::DbgCsrLocal);
    ASSERT_NE(nullptr, opt);
    EXPECT_STREQ("-cl-include-sip-kernel-local-debug -cl-include-sip-csr -cl-set-bti:0", opt);
}

TEST(DebugSip, DISABLED_givenBuiltInsWhenDbgCsrSipIsRequestedThanCorrectSipKernelIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_NE(nullptr, mockDevice);
    MockCompilerDebugVars igcDebugVars;

    std::string name = "sip_dummy_kernel_debug";
    std::string builtInFileRoot = testFiles + getDebugSipKernelNameWithBitnessAndProductSuffix(name, binaryNameSuffix.c_str());
    std::string builtInGenFile = builtInFileRoot;
    builtInGenFile.append(".gen");

    igcDebugVars.fileName = builtInGenFile;
    gEnvironment->igcPushDebugVars(igcDebugVars);

    auto &builtins = mockDevice->getBuiltIns();
    auto &sipKernel = builtins.getSipKernel(SipKernelType::DbgCsr, *mockDevice);

    EXPECT_NE(nullptr, &sipKernel);
    EXPECT_EQ(SipKernelType::DbgCsr, sipKernel.getType());
    gEnvironment->igcPopDebugVars();
}

} // namespace SipKernelTests
