/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/device/device.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clSVMAllocTests;

namespace ULT {

class clSVMAllocTemplateTests : public api_fixture,
                                public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    void SetUp() override {
        api_fixture::SetUp();
        if (!pPlatform->peekExecutionEnvironment()->getHardwareInfo()->capabilityTable.ftrSvm) {
            GTEST_SKIP();
        }
    }

    void TearDown() override {
        api_fixture::TearDown();
    }
};

struct clSVMAllocValidFlagsTests : public clSVMAllocTemplateTests {
    cl_uchar pHostPtr[64];
};

TEST_P(clSVMAllocValidFlagsTests, SVMAllocValidFlags) {
    cl_mem_flags flags = GetParam();
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    //check for svm support
    if (devInfo.svmCapabilities != 0) {
        //fg svm flag
        if (flags & CL_MEM_SVM_FINE_GRAIN_BUFFER) {
            //fg svm flag, fg svm support - expected success
            if (devInfo.svmCapabilities & CL_DEVICE_SVM_FINE_GRAIN_BUFFER) {
                auto SVMPtr = clSVMAlloc(pContext, flags, 4096 /* Size*/, 128 /* alignment */);
                EXPECT_NE(nullptr, SVMPtr);

                clSVMFree(pContext, SVMPtr);
            }
            //fg svm flag no fg svm support
            else {
                auto SVMPtr = clSVMAlloc(pContext, flags, 4096 /* Size*/, 128 /* alignment */);
                EXPECT_EQ(nullptr, SVMPtr);
            }
        }
        //no fg svm flag, svm support - expected success
        else {
            auto SVMPtr = clSVMAlloc(pContext, flags, 4096 /* Size*/, 128 /* alignment */);
            EXPECT_NE(nullptr, SVMPtr);

            clSVMFree(pContext, SVMPtr);
        }
    } else {
        //no svm support -expected fail
        auto SVMPtr = clSVMAlloc(pContext, flags, 4096 /* Size*/, 128 /* alignment */);
        EXPECT_EQ(nullptr, SVMPtr);
    }
};

static cl_mem_flags SVMAllocValidFlags[] = {
    0,
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_SVM_FINE_GRAIN_BUFFER,
    CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS,
    CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER,
    CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS,
    CL_MEM_WRITE_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER,
    CL_MEM_WRITE_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS,
    CL_MEM_READ_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER,
    CL_MEM_READ_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS};

INSTANTIATE_TEST_CASE_P(
    SVMAllocCheckFlags,
    clSVMAllocValidFlagsTests,
    testing::ValuesIn(SVMAllocValidFlags));

using clSVMAllocFtrFlagsTests = clSVMAllocTemplateTests;

INSTANTIATE_TEST_CASE_P(
    SVMAllocCheckFlagsFtrFlags,
    clSVMAllocFtrFlagsTests,
    testing::ValuesIn(SVMAllocValidFlags));

TEST_P(clSVMAllocFtrFlagsTests, SVMAllocValidFlags) {
    HardwareInfo *pHwInfo = pPlatform->peekExecutionEnvironment()->getMutableHardwareInfo();

    cl_mem_flags flags = GetParam();
    void *SVMPtr = nullptr;

    //1: no svm - no flags supported
    pHwInfo->capabilityTable.ftrSvm = false;
    pHwInfo->capabilityTable.ftrSupportsCoherency = false;

    SVMPtr = clSVMAlloc(pContext, flags, 4096, 128);
    EXPECT_EQ(nullptr, SVMPtr);

    //2: coarse svm - normal flags supported
    pHwInfo->capabilityTable.ftrSvm = true;
    SVMPtr = clSVMAlloc(pContext, flags, 4096, 128);
    if (flags & CL_MEM_SVM_FINE_GRAIN_BUFFER) {
        //fg svm flags not supported
        EXPECT_EQ(nullptr, SVMPtr);
    } else {
        //no fg svm flags supported
        EXPECT_NE(nullptr, SVMPtr);
        clSVMFree(pContext, SVMPtr);
    }

    //3: fg svm - all flags supported
    pHwInfo->capabilityTable.ftrSupportsCoherency = true;
    SVMPtr = clSVMAlloc(pContext, flags, 4096, 128);
    EXPECT_NE(nullptr, SVMPtr);
    clSVMFree(pContext, SVMPtr);
};

struct clSVMAllocInvalidFlagsTests : public clSVMAllocTemplateTests {
};

TEST_P(clSVMAllocInvalidFlagsTests, SVMAllocInvalidFlags) {
    cl_mem_flags flags = GetParam();

    auto SVMPtr = clSVMAlloc(pContext, flags, 4096 /* Size*/, 128 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
};

cl_mem_flags SVMAllocInvalidFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
    CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY,
    CL_MEM_SVM_ATOMICS,
    0xffcc};

INSTANTIATE_TEST_CASE_P(
    SVMAllocCheckFlags,
    clSVMAllocInvalidFlagsTests,
    testing::ValuesIn(SVMAllocInvalidFlags));

TEST_F(clSVMAllocTests, nullContextReturnsNull) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto SVMPtr = clSVMAlloc(0 /* cl_context */, flags, 4096 /* Size*/, 128 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
}

TEST_F(clSVMAllocTests, zeroSizeReturnsNull) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 0 /* Size*/, 128 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
}

TEST_F(clSVMAllocTests, zeroAlignmentReturnsPtr) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 4096 /* Size*/, 0 /* alignment */);
        EXPECT_NE(nullptr, SVMPtr);
        clSVMFree(pContext, SVMPtr);
    }
}

TEST_F(clSVMAllocTests, sizeNotAlignReturnsPtr) {
    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 4095 /* Size*/, 0 /* alignment */);
        EXPECT_NE(nullptr, SVMPtr);
        clSVMFree(pContext, SVMPtr);
    }
}

TEST_F(clSVMAllocTests, AlignmentNotPowerOf2ReturnsNull) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 4096 /* Size*/, 129 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
}

TEST_F(clSVMAllocTests, SVMAllocAlignmentToLarge) {
    auto SVMPtr = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 4096 /* Size */, 4096 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
};
} // namespace ULT
