/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCreateBufferTests = ApiTests;

namespace ULT {

class ClCreateBufferTemplateTests : public ApiFixture<>,
                                    public testing::TestWithParam<uint64_t> {
    void SetUp() override {
        ApiFixture::setUp();
    }

    void TearDown() override {
        ApiFixture::tearDown();
    }
};

struct ClCreateBufferValidFlagsTests : public ClCreateBufferTemplateTests {
    cl_uchar pHostPtr[64];
};

TEST_P(ClCreateBufferValidFlagsTests, GivenValidFlagsWhenCreatingBufferThenBufferIsCreated) {
    cl_mem_flags flags = GetParam() | CL_MEM_USE_HOST_PTR;

    auto buffer = clCreateBuffer(pContext, flags, 64, pHostPtr, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseMemObject(buffer);

    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, flags, 0};
    buffer = clCreateBufferWithPropertiesINTEL(pContext, properties, 0, 64, pHostPtr, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseMemObject(buffer);

    buffer = clCreateBufferWithPropertiesINTEL(pContext, nullptr, flags, 64, pHostPtr, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    clReleaseMemObject(buffer);
};

static cl_mem_flags validFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS,
    CL_MEM_FORCE_HOST_MEMORY_INTEL};

INSTANTIATE_TEST_SUITE_P(
    CreateBufferCheckFlags,
    ClCreateBufferValidFlagsTests,
    testing::ValuesIn(validFlags));

using clCreateBufferInvalidFlagsTests = ClCreateBufferTemplateTests;

TEST_P(clCreateBufferInvalidFlagsTests, GivenInvalidFlagsWhenCreatingBufferThenBufferIsNotCreated) {
    cl_mem_flags flags = GetParam();

    auto buffer = clCreateBuffer(pContext, flags, 64, nullptr, &retVal);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, flags, 0};
    buffer = clCreateBufferWithPropertiesINTEL(pContext, properties, 0, 64, nullptr, &retVal);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);

    buffer = clCreateBufferWithPropertiesINTEL(pContext, nullptr, flags, 64, nullptr, &retVal);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
};

cl_mem_flags invalidFlags[] = {
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
    CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
    CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY,
    CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR,
    CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
    0xffcc,
};

INSTANTIATE_TEST_SUITE_P(
    CreateBufferCheckFlags,
    clCreateBufferInvalidFlagsTests,
    testing::ValuesIn(invalidFlags));

using clCreateBufferValidFlagsIntelTests = ClCreateBufferTemplateTests;

TEST_P(clCreateBufferValidFlagsIntelTests, GivenValidFlagsIntelWhenCreatingBufferThenBufferIsCreated) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS_INTEL, GetParam(), 0};

    auto buffer = clCreateBufferWithPropertiesINTEL(pContext, properties, 0, 64, nullptr, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseMemObject(buffer);
};

static cl_mem_flags validFlagsIntel[] = {
    CL_MEM_LOCALLY_UNCACHED_RESOURCE,
    CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE,
    CL_MEM_48BIT_RESOURCE_INTEL};

INSTANTIATE_TEST_SUITE_P(
    CreateBufferCheckFlagsIntel,
    clCreateBufferValidFlagsIntelTests,
    testing::ValuesIn(validFlagsIntel));

using clCreateBufferInvalidFlagsIntelTests = ClCreateBufferTemplateTests;

TEST_P(clCreateBufferInvalidFlagsIntelTests, GivenInvalidFlagsIntelWhenCreatingBufferThenBufferIsNotCreated) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS_INTEL, GetParam(), 0};

    auto buffer = clCreateBufferWithPropertiesINTEL(pContext, properties, 0, 64, nullptr, &retVal);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
};

cl_mem_flags invalidFlagsIntel[] = {
    0xffcc,
};

INSTANTIATE_TEST_SUITE_P(
    CreateBufferCheckFlagsIntel,
    clCreateBufferInvalidFlagsIntelTests,
    testing::ValuesIn(invalidFlagsIntel));

using clCreateBufferInvalidProperties = ClCreateBufferTemplateTests;

TEST_F(clCreateBufferInvalidProperties, GivenInvalidPropertyKeyWhenCreatingBufferThenBufferIsNotCreated) {
    cl_mem_properties_intel properties[] = {(cl_mem_properties_intel(1) << 31), 0, 0};

    auto buffer = clCreateBufferWithPropertiesINTEL(pContext, properties, 0, 64, nullptr, &retVal);
    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
};

TEST_F(ClCreateBufferTests, GivenValidParametersWhenCreatingBufferThenSuccessIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;

    unsigned char pHostMem[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    buffer = clCreateBuffer(pContext, flags, bufferSize, pHostMem, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateBufferTests, GivenForceExtendedBufferSizeDebugFlagWhenBufferIsCreatedThenSizeIsProperlyExtended) {
    DebugManagerStateRestore restorer;

    unsigned char *pHostMem = nullptr;
    cl_mem_flags flags = 0;
    constexpr auto bufferSize = 16;

    auto pageSizeNumber = 1;
    debugManager.flags.ForceExtendedBufferSize.set(pageSizeNumber);
    auto extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    auto buffer = clCreateBuffer(pContext, flags, bufferSize, pHostMem, &retVal);

    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto bufferObj = NEO::castToObject<Buffer>(buffer);
    EXPECT_EQ(extendedBufferSize, bufferObj->getSize());

    clReleaseMemObject(buffer);

    pageSizeNumber = 4;
    debugManager.flags.ForceExtendedBufferSize.set(pageSizeNumber);
    extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    buffer = clCreateBufferWithProperties(pContext, nullptr, flags, bufferSize, pHostMem, &retVal);

    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    bufferObj = NEO::castToObject<Buffer>(buffer);
    EXPECT_EQ(extendedBufferSize, bufferObj->getSize());

    clReleaseMemObject(buffer);

    pageSizeNumber = 6;
    debugManager.flags.ForceExtendedBufferSize.set(pageSizeNumber);
    extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    buffer = clCreateBufferWithPropertiesINTEL(pContext, nullptr, flags, bufferSize, pHostMem, &retVal);

    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    bufferObj = NEO::castToObject<Buffer>(buffer);
    EXPECT_EQ(extendedBufferSize, bufferObj->getSize());

    clReleaseMemObject(buffer);
}

TEST_F(ClCreateBufferTests, GivenNullContextWhenCreatingBufferThenInvalidContextErrorIsReturned) {
    unsigned char *pHostMem = nullptr;
    cl_mem_flags flags = 0;
    static const unsigned int bufferSize = 16;

    clCreateBuffer(nullptr, flags, bufferSize, pHostMem, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);

    clCreateBufferWithPropertiesINTEL(nullptr, nullptr, 0, bufferSize, pHostMem, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClCreateBufferTests, GivenBufferSizeZeroWhenCreatingBufferThenInvalidBufferSizeErrorIsReturned) {
    uint8_t hostData = 0;
    clCreateBuffer(pContext, CL_MEM_USE_HOST_PTR, 0, &hostData, &retVal);
    ASSERT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
}

TEST_F(ClCreateBufferTests, GivenInvalidHostPointerWhenCreatingBufferThenInvalidHostPointerErrorIsReturned) {
    uint32_t hostData = 0;
    cl_mem_flags flags = 0;
    clCreateBuffer(pContext, flags, sizeof(uint32_t), &hostData, &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
}

TEST_F(ClCreateBufferTests, GivenNullHostPointerAndMemCopyHostPtrFlagWhenCreatingBufferThenInvalidHostPointerErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_COPY_HOST_PTR;
    clCreateBuffer(pContext, flags, sizeof(uint32_t), nullptr, &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
}

TEST_F(ClCreateBufferTests, GivenNullHostPointerAndMemUseHostPtrFlagWhenCreatingBufferThenInvalidHostPointerErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    clCreateBuffer(pContext, flags, sizeof(uint32_t), nullptr, &retVal);
    ASSERT_EQ(CL_INVALID_HOST_PTR, retVal);
}

TEST_F(ClCreateBufferTests, GivenMemWriteOnlyFlagAndMemReadWriteFlagWhenCreatingBufferThenInvalidValueErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_READ_WRITE;
    clCreateBuffer(pContext, flags, 16, nullptr, &retVal);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClCreateBufferTests, GivenBufferSizeOverMaxMemAllocSizeWhenCreatingBufferThenInvalidBufferSizeErrorIsReturned) {
    auto pDevice = pContext->getDevice(0);
    size_t size = static_cast<size_t>(pDevice->getDevice().getDeviceInfo().maxMemAllocSize) + 1;

    auto buffer = clCreateBuffer(pContext, CL_MEM_ALLOC_HOST_PTR, size, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, buffer);
}

TEST_F(ClCreateBufferTests, GivenBufferSizeOverMaxMemAllocSizeWhenCreateBufferWithPropertiesINTELThenInvalidBufferSizeErrorIsReturned) {
    auto pDevice = pContext->getDevice(0);
    size_t size = static_cast<size_t>(pDevice->getDevice().getDeviceInfo().maxMemAllocSize) + 1;

    auto buffer = clCreateBufferWithPropertiesINTEL(pContext, nullptr, 0, size, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
    EXPECT_EQ(nullptr, buffer);
}

TEST_F(ClCreateBufferTests, GivenValidHostPointerAndSizeOverSVMAllocSizeWhenCreatingBufferThenInvalidBufferSizeErrorIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        size_t size = 10;

        const cl_mem_flags flags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR};
        cl_char *data = static_cast<cl_char *>(clSVMAlloc(pContext, CL_MEM_READ_WRITE, size, 0));

        for (const auto &flag : flags) {
            cl_mem buffer = clCreateBuffer(pContext, flag, 2 * size, data, &retVal);

            EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
            EXPECT_EQ(nullptr, buffer);
        }

        clSVMFree(pContext, data);
    }
}

TEST_F(ClCreateBufferTests, GivenValidHostPointerAndSizeWithinSVMAllocSizeWhenCreatingBufferThenSuccessIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        size_t size = 10;

        const cl_mem_flags flags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR};
        cl_char *data = static_cast<cl_char *>(clSVMAlloc(pContext, CL_MEM_READ_WRITE, size, 0));

        for (const auto &flag : flags) {
            cl_mem buffer = clCreateBuffer(pContext, flag, size, data, &retVal);

            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, buffer);

            retVal = clReleaseMemObject(buffer);
            EXPECT_EQ(CL_SUCCESS, retVal);
        }

        clSVMFree(pContext, data);
    }
}

TEST_F(ClCreateBufferTests, GivenValidHostPointerAndSVMAllocWhenOffsetAndSizeReachOutOfBoundWhenCreatingBufferThenInvalidBufferSizeIsReturned) {
    const ClDeviceInfo &devInfo = pDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        size_t size = 1024;
        size_t offset = 1020;

        const cl_mem_flags flags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR};
        cl_char *data = static_cast<cl_char *>(clSVMAlloc(pContext, CL_MEM_READ_WRITE, size, 0));

        for (const auto &flag : flags) {
            cl_mem buffer = clCreateBuffer(pContext, flag, size, (cl_char *)(data + offset), &retVal);

            EXPECT_EQ(CL_INVALID_BUFFER_SIZE, retVal);
            EXPECT_EQ(nullptr, buffer);
        }

        clSVMFree(pContext, data);
    }
}

TEST_F(ClCreateBufferTests, GivenBufferSizeOverMaxMemAllocSizeAndClMemAllowUnrestirctedSizeFlagWhenCreatingBufferThenClSuccessIsReturned) {
    auto pDevice = pContext->getDevice(0);
    uint64_t bigSize = MemoryConstants::gigaByte * 5;
    size_t size = static_cast<size_t>(bigSize);
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR | CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    if (memoryManager->peekForce32BitAllocations() || is32bit) {
        GTEST_SKIP();
    }

    auto buffer = clCreateBuffer(pContext, flags, size, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateBufferTests, GivenBufferSizeOverMaxMemAllocSizeAndClMemAllowUnrestirctedSizeFlagWhenCreatingBufferWithPropertiesINTELThenClSuccesssIsReturned) {
    auto pDevice = pContext->getDevice(0);
    uint64_t bigSize = MemoryConstants::gigaByte * 5;
    size_t size = static_cast<size_t>(bigSize);
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS_INTEL, CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL, 0};

    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    if (memoryManager->peekForce32BitAllocations() || is32bit) {
        GTEST_SKIP();
    }

    auto buffer = clCreateBufferWithPropertiesINTEL(pContext, properties, 0, size, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateBufferTests, GivenBufferSizeOverMaxMemAllocSizeAndDebugFlagSetWhenCreatingBufferThenClSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowUnrestrictedSize.set(1);
    auto pDevice = pContext->getDevice(0);
    size_t size = static_cast<size_t>(pDevice->getDevice().getDeviceInfo().maxMemAllocSize) + 1;
    auto memoryManager = static_cast<OsAgnosticMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->turnOnFakingBigAllocations();

    if (memoryManager->peekForce32BitAllocations() || is32bit) {
        GTEST_SKIP();
    }

    auto buffer = clCreateBuffer(pContext, 0, size, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateBufferTests, GivenNullHostPointerAndMemCopyHostPtrFlagWhenCreatingBufferThenNullIsReturned) {
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    static const unsigned int bufferSize = 16;
    cl_mem buffer = nullptr;

    unsigned char pHostMem[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    buffer = clCreateBuffer(pContext, flags, bufferSize, pHostMem, nullptr);

    EXPECT_NE(nullptr, buffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateBufferTests, WhenCreatingBufferWithPropertiesThenParametersAreCorrectlyPassed) {
    VariableBackup<BufferFunctions::ValidateInputAndCreateBufferFunc> bufferCreateBackup{&BufferFunctions::validateInputAndCreateBuffer};

    cl_context context = pContext;
    cl_mem_properties *propertiesValues[] = {nullptr, reinterpret_cast<cl_mem_properties *>(0x1234)};
    cl_mem_flags flagsValues[] = {0, 4321};
    size_t bufferSize = 128;
    void *pHostMem = reinterpret_cast<void *>(0x8000);

    for (auto properties : propertiesValues) {
        for (auto flags : flagsValues) {
            auto mockFunction = [context, properties, flags, bufferSize, pHostMem](cl_context contextArg,
                                                                                   const cl_mem_properties *propertiesArg,
                                                                                   cl_mem_flags flagsArg,
                                                                                   cl_mem_flags_intel flagsIntelArg,
                                                                                   size_t sizeArg,
                                                                                   void *hostPtrArg,
                                                                                   cl_int &retValArg) -> cl_mem {
                cl_mem_flags_intel expectedFlagsIntelArg = 0;

                EXPECT_EQ(context, contextArg);
                EXPECT_EQ(properties, propertiesArg);
                EXPECT_EQ(flags, flagsArg);
                EXPECT_EQ(expectedFlagsIntelArg, flagsIntelArg);
                EXPECT_EQ(bufferSize, sizeArg);
                EXPECT_EQ(pHostMem, hostPtrArg);

                return nullptr;
            };
            bufferCreateBackup = mockFunction;
            clCreateBufferWithProperties(context, properties, flags, bufferSize, pHostMem, nullptr);
        }
    }
}

TEST_F(ClCreateBufferTests, WhenCreatingBufferWithPropertiesThenErrorCodeIsCorrectlySet) {
    VariableBackup<BufferFunctions::ValidateInputAndCreateBufferFunc> bufferCreateBackup{&BufferFunctions::validateInputAndCreateBuffer};

    cl_mem_properties *properties = nullptr;
    cl_mem_flags flags = 0;
    size_t bufferSize = 128;
    void *pHostMem = nullptr;
    cl_int errcodeRet;

    cl_int retValues[] = {CL_SUCCESS, CL_INVALID_PROPERTY};

    for (auto retValue : retValues) {
        auto mockFunction = [retValue](cl_context contextArg,
                                       const cl_mem_properties *propertiesArg,
                                       cl_mem_flags flagsArg,
                                       cl_mem_flags_intel flagsIntelArg,
                                       size_t sizeArg,
                                       void *hostPtrArg,
                                       cl_int &retValArg) -> cl_mem {
            retValArg = retValue;

            return nullptr;
        };
        bufferCreateBackup = mockFunction;
        clCreateBufferWithProperties(pContext, properties, flags, bufferSize, pHostMem, &errcodeRet);
        EXPECT_EQ(retValue, errcodeRet);
    }
}

TEST_F(ClCreateBufferTests, GivenBufferCreatedWithNullPropertiesWhenQueryingPropertiesThenNothingIsReturned) {
    cl_int retVal = CL_SUCCESS;
    size_t size = 10;
    auto buffer = clCreateBufferWithPropertiesINTEL(pContext, nullptr, 0, size, nullptr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, buffer);

    size_t propertiesSize;
    retVal = clGetMemObjectInfo(buffer, CL_MEM_PROPERTIES, 0, nullptr, &propertiesSize);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(0u, propertiesSize);

    clReleaseMemObject(buffer);
}

TEST_F(ClCreateBufferTests, WhenCreatingBufferWithPropertiesThenPropertiesAreCorrectlyStored) {
    cl_int retVal = CL_SUCCESS;
    size_t size = 10;
    cl_mem_properties properties[5];
    size_t propertiesSize;

    std::vector<std::vector<uint64_t>> propertiesToTest{
        {0},
        {CL_MEM_FLAGS, CL_MEM_WRITE_ONLY, 0},
        {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0},
        {CL_MEM_FLAGS, CL_MEM_WRITE_ONLY, CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0}};

    for (auto testProperties : propertiesToTest) {
        auto buffer = clCreateBufferWithPropertiesINTEL(pContext, testProperties.data(), 0, size, nullptr, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, buffer);

        retVal = clGetMemObjectInfo(buffer, CL_MEM_PROPERTIES, sizeof(properties), properties, &propertiesSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(testProperties.size() * sizeof(cl_mem_properties), propertiesSize);
        for (size_t i = 0; i < testProperties.size(); i++) {
            EXPECT_EQ(testProperties[i], properties[i]);
        }

        retVal = clReleaseMemObject(buffer);
    }
}

using clCreateBufferTestsWithRestrictions = api_test_using_aligned_memory_manager;

TEST_F(clCreateBufferTestsWithRestrictions, GivenMemoryManagerRestrictionsWhenMinIsLessThanHostPtrThenUseZeroCopy) {
    std::unique_ptr<unsigned char[]> hostMem(nullptr);
    unsigned char *destMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    const unsigned int bufferSize = MemoryConstants::pageSize * 3;
    const unsigned int destBufferSize = MemoryConstants::pageSize;
    cl_mem buffer = nullptr;

    uintptr_t minAddress = 0;
    MockAllocSysMemAgnosticMemoryManager *memMngr = reinterpret_cast<MockAllocSysMemAgnosticMemoryManager *>(device->getMemoryManager());
    memMngr->ptrRestrictions = &memMngr->testRestrictions;
    EXPECT_EQ(minAddress, memMngr->ptrRestrictions->minAddress);

    hostMem.reset(new unsigned char[bufferSize]);

    destMem = hostMem.get();
    destMem += MemoryConstants::pageSize;
    destMem -= (reinterpret_cast<uintptr_t>(destMem) % MemoryConstants::pageSize);

    buffer = clCreateBuffer(context, flags, destBufferSize, destMem, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    Buffer *bufferObj = NEO::castToObject<Buffer>(buffer);
    EXPECT_TRUE(bufferObj->isMemObjZeroCopy());

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateBufferTestsWithRestrictions, GivenMemoryManagerRestrictionsWhenMinIsLessThanHostPtrThenCreateCopy) {
    std::unique_ptr<unsigned char[]> hostMem(nullptr);
    unsigned char *destMem = nullptr;
    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    const unsigned int realBufferSize = MemoryConstants::pageSize * 3;
    const unsigned int destBufferSize = MemoryConstants::pageSize;
    cl_mem buffer = nullptr;

    MockAllocSysMemAgnosticMemoryManager *memMngr = reinterpret_cast<MockAllocSysMemAgnosticMemoryManager *>(device->getMemoryManager());
    memMngr->ptrRestrictions = &memMngr->testRestrictions;

    hostMem.reset(new unsigned char[realBufferSize]);

    destMem = hostMem.get();
    destMem += MemoryConstants::pageSize;
    destMem -= (reinterpret_cast<uintptr_t>(destMem) % MemoryConstants::pageSize);
    memMngr->ptrRestrictions->minAddress = reinterpret_cast<uintptr_t>(destMem) + 1;

    buffer = clCreateBuffer(context, flags, destBufferSize, destMem, &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    Buffer *bufferObj = NEO::castToObject<Buffer>(buffer);
    EXPECT_FALSE(bufferObj->isMemObjZeroCopy());

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

using clCreateBufferWithMultiDeviceContextTests = ClCreateBufferTemplateTests;

TEST_P(clCreateBufferWithMultiDeviceContextTests, GivenBufferCreatedWithContextdWithMultiDeviceThenGraphicsAllocationsAreProperlyFilled) {
    UltClDeviceFactory deviceFactory{2, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);

    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1]};
    auto context = clCreateContext(nullptr, 2u, devices, nullptr, nullptr, &retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pContext = castToObject<Context>(context);

    EXPECT_EQ(1u, pContext->getMaxRootDeviceIndex());

    constexpr auto bufferSize = 64u;

    auto hostBuffer = alignedMalloc(bufferSize, MemoryConstants::pageSize64k);
    auto ptrHostBuffer = static_cast<uint8_t *>(hostBuffer);

    cl_mem_flags flags = GetParam();

    auto buffer = clCreateBuffer(context, flags, bufferSize, flags == 0 ? nullptr : ptrHostBuffer, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    Buffer *bufferObj = NEO::castToObject<Buffer>(buffer);

    EXPECT_EQ(bufferObj->getMultiGraphicsAllocation().getGraphicsAllocations().size(), 2u);
    EXPECT_NE(bufferObj->getMultiGraphicsAllocation().getGraphicsAllocation(0u), nullptr);
    EXPECT_NE(bufferObj->getMultiGraphicsAllocation().getGraphicsAllocation(1u), nullptr);
    EXPECT_NE(bufferObj->getMultiGraphicsAllocation().getGraphicsAllocation(0u), bufferObj->getMultiGraphicsAllocation().getGraphicsAllocation(1u));

    alignedFree(hostBuffer);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseContext(context);
}

static cl_mem_flags validFlagsForMultiDeviceContextBuffer[] = {
    CL_MEM_USE_HOST_PTR,
    CL_MEM_COPY_HOST_PTR, 0};

INSTANTIATE_TEST_SUITE_P(
    CreateBufferWithMultiDeviceContextCheckFlags,
    clCreateBufferWithMultiDeviceContextTests,
    testing::ValuesIn(validFlagsForMultiDeviceContextBuffer));

using clCreateBufferWithMultiDeviceContextFaillingAllocationTests = ClCreateBufferTemplateTests;

TEST_F(clCreateBufferWithMultiDeviceContextFaillingAllocationTests, GivenContextdWithMultiDeviceFailingAllocationThenBufferAllocateFails) {
    UltClDeviceFactory deviceFactory{3, 0};
    debugManager.flags.EnableMultiRootDeviceContexts.set(true);

    cl_device_id devices[] = {deviceFactory.rootDevices[0], deviceFactory.rootDevices[1], deviceFactory.rootDevices[2]};

    MockContext pContext(ClDeviceVector(devices, 3));

    EXPECT_EQ(2u, pContext.getMaxRootDeviceIndex());

    constexpr auto bufferSize = 64u;

    auto hostBuffer = alignedMalloc(bufferSize, MemoryConstants::pageSize64k);
    auto ptrHostBuffer = static_cast<uint8_t *>(hostBuffer);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;

    static_cast<MockMemoryManager *>(pContext.memoryManager)->successAllocatedGraphicsMemoryIndex = 0u;
    static_cast<MockMemoryManager *>(pContext.memoryManager)->maxSuccessAllocatedGraphicsMemoryIndex = 2u;

    auto buffer = clCreateBuffer(&pContext, flags, bufferSize, ptrHostBuffer, &retVal);
    ASSERT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(nullptr, buffer);

    alignedFree(hostBuffer);
}

} // namespace ULT
