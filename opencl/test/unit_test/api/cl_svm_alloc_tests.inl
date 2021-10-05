/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clSVMAllocTests;

namespace ULT {

class clSVMAllocTemplateTests : public ApiFixture<>,
                                public testing::TestWithParam<uint64_t /*cl_mem_flags*/> {
  public:
    void SetUp() override {
        ApiFixture::SetUp();
        REQUIRE_SVM_OR_SKIP(pDevice);
    }

    void TearDown() override {
        ApiFixture::TearDown();
    }
};

struct clSVMAllocValidFlagsTests : public clSVMAllocTemplateTests {
    cl_uchar pHostPtr[64];
};

TEST(clSVMAllocTest, givenPlatformWithoutDevicesWhenClSVMAllocIsCalledThenDeviceIsTakenFromContext) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto clDevice = std::make_unique<ClDevice>(*Device::create<RootDevice>(executionEnvironment, 0u), platform());
    const ClDeviceInfo &devInfo = clDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    cl_device_id deviceId = clDevice.get();
    cl_int retVal;
    auto context = ReleaseableObjectPtr<Context>(Context::create<Context>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, platform()->getNumDevices());
    auto SVMPtr = clSVMAlloc(context.get(), 0u, 4096, 128);
    EXPECT_NE(nullptr, SVMPtr);

    clSVMFree(context.get(), SVMPtr);
}

TEST_P(clSVMAllocValidFlagsTests, GivenSvmSupportWhenAllocatingSvmThenSvmIsAllocated) {
    cl_mem_flags flags = GetParam();
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
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

TEST_P(clSVMAllocFtrFlagsTests, GivenCorrectFlagsWhenAllocatingSvmThenSvmIsAllocated) {
    HardwareInfo *pHwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[testedRootDeviceIndex]->getMutableHardwareInfo();

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

TEST_P(clSVMAllocInvalidFlagsTests, GivenInvalidFlagsWhenAllocatingSvmThenSvmIsNotAllocated) {
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

TEST_F(clSVMAllocTests, GivenNullContextWhenAllocatingSvmThenSvmIsNotAllocated) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto SVMPtr = clSVMAlloc(nullptr /* cl_context */, flags, 4096 /* Size*/, 128 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
}

TEST_F(clSVMAllocTests, GivenZeroSizeWhenAllocatingSvmThenSvmIsNotAllocated) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 0 /* Size*/, 128 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
}

TEST_F(clSVMAllocTests, GivenZeroAlignmentWhenAllocatingSvmThenSvmIsAllocated) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 4096 /* Size*/, 0 /* alignment */);
        EXPECT_NE(nullptr, SVMPtr);
        clSVMFree(pContext, SVMPtr);
    }
}

TEST_F(clSVMAllocTests, givenUnrestrictedFlagWhenCreatingSvmAllocThenAllowSizeBiggerThanMaxMemAllocSize) {
    REQUIRE_SVM_OR_SKIP(pDevice);

    const size_t maxMemAllocSize = 128;

    static_cast<MockDevice &>(pDevice->getDevice()).deviceInfo.maxMemAllocSize = maxMemAllocSize;

    size_t allowedSize = maxMemAllocSize;
    size_t notAllowedSize = maxMemAllocSize + 1;

    cl_mem_flags flags = 0;
    void *svmPtr = nullptr;

    {
        // no flag + not allowed size
        svmPtr = clSVMAlloc(pContext, flags, notAllowedSize, 0);
        EXPECT_EQ(nullptr, svmPtr);
    }

    flags = CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;

    {
        // unrestricted size flag + not allowed size
        svmPtr = clSVMAlloc(pContext, flags, notAllowedSize, 0);
        EXPECT_NE(nullptr, svmPtr);
        clSVMFree(pContext, svmPtr);
    }

    {
        // debug flag + not allowed size
        DebugManagerStateRestore restorer;
        DebugManager.flags.AllowUnrestrictedSize.set(1);
        svmPtr = clSVMAlloc(pContext, 0, notAllowedSize, 0);
        EXPECT_NE(nullptr, svmPtr);
        clSVMFree(pContext, svmPtr);
    }

    {
        // unrestricted size flag + allowed size
        svmPtr = clSVMAlloc(pContext, flags, allowedSize, 0);
        EXPECT_NE(nullptr, svmPtr);
        clSVMFree(pContext, svmPtr);
    }
}

TEST_F(clSVMAllocTests, GivenUnalignedSizeAndDefaultAlignmentWhenAllocatingSvmThenSvmIsAllocated) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 4095 /* Size*/, 0 /* alignment */);
        EXPECT_NE(nullptr, SVMPtr);
        clSVMFree(pContext, SVMPtr);
    }
}

TEST_F(clSVMAllocTests, GivenAlignmentNotPowerOfTwoWhenAllocatingSvmThenSvmIsNotAllocated) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto SVMPtr = clSVMAlloc(pContext /* cl_context */, flags, 4096 /* Size*/, 129 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
}

TEST_F(clSVMAllocTests, GivenAlignmentTooLargeWhenAllocatingSvmThenSvmIsNotAllocated) {
    auto SVMPtr = clSVMAlloc(pContext, CL_MEM_READ_WRITE, 4096 /* Size */, 4096 /* alignment */);
    EXPECT_EQ(nullptr, SVMPtr);
};

TEST_F(clSVMAllocTests, GivenForcedFineGrainedSvmWhenCreatingSvmAllocThenAllocationIsCreated) {
    REQUIRE_SVM_OR_SKIP(pDevice);
    DebugManagerStateRestore restore{};
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[testedRootDeviceIndex]->getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrSvm = true;
    hwInfo->capabilityTable.ftrSupportsCoherency = false;

    auto allocation = clSVMAlloc(pContext, CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER, 4096 /* Size */, 0 /* alignment */);
    EXPECT_EQ(nullptr, allocation);
    clSVMFree(pContext, allocation);

    DebugManager.flags.ForceFineGrainedSVMSupport.set(1);
    allocation = clSVMAlloc(pContext, CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER, 4096 /* Size */, 0 /* alignment */);
    EXPECT_NE(nullptr, allocation);
    clSVMFree(pContext, allocation);
}

TEST(clSvmAllocTest, givenSubDeviceWhenCreatingSvmAllocThenProperDeviceBitfieldIsPassed) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo.get());
    UltClDeviceFactory deviceFactory{1, 2};
    auto device = deviceFactory.subDevices[1];

    auto executionEnvironment = device->getExecutionEnvironment();
    auto memoryManager = new MockMemoryManager(*executionEnvironment);

    std::unique_ptr<MemoryManager> memoryManagerBackup(memoryManager);
    std::swap(memoryManagerBackup, executionEnvironment->memoryManager);

    MockContext context(device);
    auto expectedDeviceBitfield = context.getDeviceBitfieldForAllocation(device->getRootDeviceIndex());
    EXPECT_NE(expectedDeviceBitfield, memoryManager->recentlyPassedDeviceBitfield);
    auto svmPtr = clSVMAlloc(&context, CL_MEM_READ_WRITE, MemoryConstants::pageSize, MemoryConstants::cacheLineSize);
    EXPECT_NE(nullptr, svmPtr);
    EXPECT_EQ(expectedDeviceBitfield, memoryManager->recentlyPassedDeviceBitfield);
    clSVMFree(&context, svmPtr);

    std::swap(memoryManagerBackup, executionEnvironment->memoryManager);
}
} // namespace ULT
