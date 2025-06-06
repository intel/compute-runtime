/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

extern int handlePrelimRequests(DrmIoctl request, void *arg, int ioctlRetVal, int queryDistanceIoctlRetVal);

class DrmPrelimMock : public DrmMock {
  public:
    using DrmMock::DrmMock;

    int ioctlRetVal = 0;
    int queryDistanceIoctlRetVal = 0;

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }
    int handleRemainingRequests(DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::query && arg != nullptr) {
            auto queryArg = static_cast<Query *>(arg);
            for (auto i = 0u; i < queryArg->numItems; i++) {
                auto queryItemArg = (reinterpret_cast<QueryItem *>(queryArg->itemsPtr) + i);
                if (queryItemArg->queryId == PRELIM_DRM_I915_QUERY_HW_IP_VERSION) {
                    ioctlCallsCount--;
                    if (queryItemArg->length == 0) {
                        queryItemArg->length = static_cast<int32_t>(sizeof(prelim_drm_i915_query_hw_ip_version));
                        if (this->returnInvalidHwIpVersionLength) {
                            queryItemArg->length -= 1;
                        }
                    } else {
                        if (this->failRetHwIpVersion) {
                            return EINVAL;
                        }
                        auto hwIpVersion = reinterpret_cast<prelim_drm_i915_query_hw_ip_version *>(queryItemArg->dataPtr);
                        hwIpVersion->stepping = 1;
                        hwIpVersion->release = 2;
                        hwIpVersion->arch = 3;
                    }
                    return 0;
                }
            }
        }
        return handlePrelimRequests(request, arg, ioctlRetVal, queryDistanceIoctlRetVal);
    }
};

class IoctlHelperPrelimFixture : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        drm->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*drm);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmPrelimMock> drm;
};

TEST(IoctlHelperPrelimTest, whenGettingVmBindAvailabilityThenProperValueIsReturnedBasedOnIoctlResult) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    const auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    IoctlHelperPrelim20 ioctlHelper{drm};

    for (auto &ioctlValue : {0, EINVAL}) {
        drm.context.vmBindQueryReturn = ioctlValue;
        for (auto &hasVmBind : ::testing::Bool()) {
            drm.context.vmBindQueryValue = hasVmBind;
            drm.context.vmBindQueryCalled = 0u;

            if (productHelper.isNewResidencyModelSupported()) {
                if (ioctlValue == 0) {
                    EXPECT_EQ(hasVmBind, ioctlHelper.isVmBindAvailable());
                } else {
                    EXPECT_FALSE(ioctlHelper.isVmBindAvailable());
                }
                EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
            } else {
                EXPECT_FALSE(ioctlHelper.isVmBindAvailable());
                EXPECT_EQ(0u, drm.context.vmBindQueryCalled);
            }
        }
    }
}

TEST(IoctlHelperPrelimTest, whenVmBindIsCalledThenProperValueIsReturnedBasedOnIoctlResult) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    IoctlHelperPrelim20 ioctlHelper{drm};

    VmBindParams vmBindParams{};

    for (auto &ioctlValue : {0, EINVAL}) {
        drm.context.vmBindReturn = ioctlValue;
        drm.context.vmBindCalled = 0u;
        EXPECT_EQ(ioctlValue, ioctlHelper.vmBind(vmBindParams));
        EXPECT_EQ(1u, drm.context.vmBindCalled);
    }
}

TEST(IoctlHelperPrelimTest, whenVmBindIsCalledThenProperCanonicalOrNonCanonicalAddressIsExpectedInVmBindInputsList) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    IoctlHelperPrelim20 ioctlHelper{drm};
    uint32_t idx = 0;

    auto testAddress = [&](uint64_t addr) {
        UserFenceVmBindExt userFence{};
        userFence.addr = idx + 1;

        VmBindParams vmBindParams{};
        vmBindParams.userFence = castToUint64(&userFence);
        vmBindParams.handle = idx + 1;
        vmBindParams.userptr = idx + 1;
        vmBindParams.start = addr;

        auto ret = ioctlHelper.vmBind(vmBindParams);
        EXPECT_EQ(0, ret);

        auto &list = drm.vmBindInputs;
        EXPECT_EQ(list.size(), idx + 1);

        if (list.size() == idx + 1) {
            EXPECT_EQ(list[idx].start, vmBindParams.start);
        }

        idx++;
    };

    testAddress(0xfffff00000000000); // canonical address test
    testAddress(0xf00000000000);     // non-canonical address test
}

TEST(IoctlHelperPrelimTest, whenVmUnbindIsCalledThenProperValueIsReturnedBasedOnIoctlResult) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    IoctlHelperPrelim20 ioctlHelper{drm};

    VmBindParams vmBindParams{};

    for (auto &ioctlValue : {0, EINVAL}) {
        drm.context.vmUnbindReturn = ioctlValue;
        drm.context.vmUnbindCalled = 0u;
        EXPECT_EQ(ioctlValue, ioctlHelper.vmUnbind(vmBindParams));
        EXPECT_EQ(1u, drm.context.vmUnbindCalled);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimEnableEuDebugThenReturnCorrectValue) {
    VariableBackup<size_t> mockFreadReturnBackup(&IoFunctions::mockFreadReturn, 1);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(IoFunctions::mockFreadReturn);
    VariableBackup<char *> mockFreadBufferBackup(&IoFunctions::mockFreadBuffer, buffer.get());

    buffer[0] = '1';
    int prelimEnableEuDebug = drm->getEuDebugSysFsEnable();

    EXPECT_EQ(1, prelimEnableEuDebug);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimEnableEuDebugWithInvalidPathThenReturnDefaultValue) {
    VariableBackup<size_t> mockFreadReturnBackup(&IoFunctions::mockFreadReturn, 1);
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(IoFunctions::mockFreadReturn);
    VariableBackup<char *> mockFreadBufferBackup(&IoFunctions::mockFreadBuffer, buffer.get());

    buffer[0] = '1';
    VariableBackup<FILE *> mockFopenReturnBackup(&IoFunctions::mockFopenReturned, nullptr);
    int prelimEnableEuDebug = drm->getEuDebugSysFsEnable();

    EXPECT_EQ(0, prelimEnableEuDebug);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtThenReturnSuccess) {
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    uint32_t numOfChunks = 0;
    auto ret = ioctlHelper->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt, std::nullopt);

    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenAtomicAccessModeHostWhenCallGetAtomicAccessReturnZero) {
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t ret = ioctlHelper->getAtomicAccess(AtomicAccessMode::host);
    EXPECT_EQ(0u, ret);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtWithChunkingThenGetNumOfChunks) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOChunkingLogs.set(true);
    debugManager.flags.NumberOfBOChunks.set(2);
    size_t allocSize = 2 * MemoryConstants::pageSize64k;

    StreamCapture capture;
    capture.captureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    uint32_t getNumOfChunks = 2;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    ioctlHelper->createGemExt(memClassInstance, allocSize, handle, 0, {}, -1, true, getNumOfChunks, std::nullopt, std::nullopt, std::nullopt);
    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("GEM_CREATE_EXT BO-1 with BOChunkingSize 65536, chunkingParamRegion.param.data 65536, numOfChunks 2\n");
    EXPECT_EQ(expectedOutput, output);
    EXPECT_EQ(2u, getNumOfChunks);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtWithChunkingAndAllocTooSmallThenExceptionThrown) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOChunkingLogs.set(false);
    debugManager.flags.NumberOfBOChunks.set(2);
    size_t allocSize = MemoryConstants::pageSize64k;

    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    uint32_t getNumOfChunks = 2;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    EXPECT_THROW(ioctlHelper->createGemExt(memClassInstance, allocSize, handle, 0, {}, -1, true, getNumOfChunks, std::nullopt, std::nullopt, std::nullopt), std::runtime_error);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    StreamCapture capture;
    capture.captureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    uint32_t numOfChunks = 0;
    ioctlHelper->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt, std::nullopt);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, param: 0x1000000010001, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCallIoctlThenProperIoctlRegistered) {
    GemContextCreateExt arg{};
    drm->ioctlCallsCount = 0;
    auto ret = drm->ioctlHelper->ioctl(DrmIoctl::gemContextCreateExt, &arg);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosAllocThenReturnCorrectRegion) {
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(NEO::CacheLevel::level3);

    EXPECT_EQ(CacheRegion::region1, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocThenReturnNone) {
    drm->ioctlRetVal = -1;
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(NEO::CacheLevel::level3);

    EXPECT_EQ(CacheRegion::none, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosFreeThenReturnCorrectRegion) {
    auto ioctlHelper = drm->getIoctlHelper();
    drm->ioctlCallsCount = 0;
    auto cacheRegion = ioctlHelper->closFree(CacheRegion::region2);

    EXPECT_EQ(CacheRegion::region2, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosFreeThenReturnNone) {
    drm->ioctlRetVal = -1;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(CacheRegion::region2);

    EXPECT_EQ(CacheRegion::none, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosAllocWaysThenReturnCorrectRegion) {
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    auto numWays = ioctlHelper->closAllocWays(CacheRegion::region2, 3, 10);

    EXPECT_EQ(10u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocWaysThenReturnNone) {
    drm->ioctlRetVal = -1;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    auto numWays = ioctlHelper->closAllocWays(CacheRegion::region2, 3, 10);

    EXPECT_EQ(0u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenWaitUserFenceThenCorrectValueReturned) {
    uint64_t gpuAddress = 0x1020304000ull;
    uint64_t value = 0x98765ull;
    auto ioctlHelper = drm->getIoctlHelper();
    for (uint32_t i = 0u; i < 4; i++) {
        auto ret = ioctlHelper->waitUserFence(10u, gpuAddress, value, i, -1, 0u, false, NEO::InterruptId::notUsed, nullptr);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseFailsThenDontUpdateMemAdviceFlags) {
    drm->ioctlCallsCount = 0;
    drm->ioctlRetVal = -1;

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};
    memAdviseFlags.nonAtomic = 1;

    allocation.setMemAdvise(drm.get(), memAdviseFlags);

    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_NE(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseForChunkingFailsThenDontUpdateMemAdviceFlags) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    drm->ioctlCallsCount = 0;
    drm->ioctlRetVal = -1;

    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};
    memAdviseFlags.nonAtomic = 1;
    allocation.storageInfo.isChunked = 1;

    allocation.setMemAdvise(drm.get(), memAdviseFlags);

    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_NE(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithNonAtomicIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    drm->ioctlCallsCount = 0;
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};

    for (auto nonAtomic : {true, false}) {
        memAdviseFlags.nonAtomic = nonAtomic;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithDevicePreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.devicePreferredLocation = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithSystemPreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    MemAdviseFlags memAdviseFlags{};

    for (auto systemPreferredLocation : {true, false}) {
        memAdviseFlags.systemPreferredLocation = systemPreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithChunkingPreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferChunks) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.devicePreferredLocation = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
    }
    EXPECT_EQ(allocation.storageInfo.numOfChunks * 2, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithChunkingButWithoutEnableBOChunkingPreferredLocationHintCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(0);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.devicePreferredLocation = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.allFlags, allocation.enabledMemAdviseFlags.allFlags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture,
       givenDrmAllocationWhenSetMemAdviseWithChunkingPreferredLocationIsCalledWithFailureThenReturnFalse) {
    drm->ioctlRetVal = -1;
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x5;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;

    MemAdviseFlags memAdviseFlags{};

    memAdviseFlags.devicePreferredLocation = true;

    EXPECT_FALSE(allocation.setMemAdvise(drm.get(), memAdviseFlags));

    EXPECT_EQ(allocation.storageInfo.numOfChunks, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetAtomicAccessWithModeCalledThenIoctlCalled) {
    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    size_t size = 16;
    AtomicAccessMode mode = AtomicAccessMode::none;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    mode = AtomicAccessMode::device;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(3u, drm->ioctlCallsCount);

    mode = AtomicAccessMode::host;
    // No IOCTL call for Host mode
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(3u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetAtomicAccessWithNullBufferObjectThenIoctlNotCalled) {
    drm->ioctlCallsCount = 0;
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = nullptr;
    allocation.storageInfo.memoryBanks = 0x1;
    allocation.setNumHandles(1);

    size_t size = 16;
    AtomicAccessMode mode = AtomicAccessMode::none;
    EXPECT_TRUE(allocation.setAtomicAccess(drm.get(), size, mode));
    EXPECT_EQ(0u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemPrefetchSucceedsThenReturnTrue) {
    SubDeviceIdsVec subDeviceIds{0};
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    drm->ioctlRetVal = 0;
    EXPECT_TRUE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemPrefetchFailsThenReturnFalse) {
    SubDeviceIdsVec subDeviceIds{0};
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.setNumHandles(1);

    drm->ioctlRetVal = EINVAL;
    EXPECT_FALSE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(IoctlHelperPrelimFixture,
       givenDrmAllocationWithChunkingAndsetMemPrefetchCalledSuccessIsReturned) {
    SubDeviceIdsVec subDeviceIds{0, 1};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPrefetch.set(true);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    bo.setChunked(true);
    bo.setSize(1024);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x5;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;
    allocation.storageInfo.subDeviceBitfield = 0b0001;
    EXPECT_TRUE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(IoctlHelperPrelimFixture,
       givenDrmAllocationWithChunkingAndsetMemPrefetchWithIoctlFailureThenFailureReturned) {
    SubDeviceIdsVec subDeviceIds{0, 1};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableBOChunking.set(1);
    debugManager.flags.EnableBOChunkingPrefetch.set(true);
    debugManager.flags.EnableBOChunkingPreferredLocationHint.set(true);

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, MemoryConstants::chunkThreshold * 4, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, MemoryConstants::chunkThreshold * 4, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));

    drm->ioctlCallsCount = 0;
    MockBufferObject bo(0u, drm.get(), 3, 0, 0, 1);
    bo.setChunked(true);
    bo.setSize(1024);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.memoryBanks = 0x5;
    allocation.setNumHandles(1);
    allocation.storageInfo.isChunked = 1;
    allocation.storageInfo.numOfChunks = 4;
    allocation.storageInfo.subDeviceBitfield = 0b0001;
    drm->ioctlRetVal = EINVAL;
    EXPECT_FALSE(allocation.setMemPrefetch(drm.get(), subDeviceIds));
}

TEST_F(IoctlHelperPrelimFixture, givenVariousDirectSubmissionFlagSettingWhenCreateDrmContextIsCalledThenCorrectFlagsArePassedToIoctl) {
    DebugManagerStateRestore stateRestore;
    uint32_t vmId = 0u;
    constexpr bool isCooperativeContextRequested = false;
    bool isDirectSubmissionRequested{};
    uint32_t ioctlVal = (1u << 31);

    debugManager.flags.DirectSubmissionDrmContext.set(-1);
    drm->receivedContextCreateFlags = 0;
    isDirectSubmissionRequested = true;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(ioctlVal, drm->receivedContextCreateFlags);

    debugManager.flags.DirectSubmissionDrmContext.set(0);
    drm->receivedContextCreateFlags = 0;
    isDirectSubmissionRequested = true;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(0u, drm->receivedContextCreateFlags);

    debugManager.flags.DirectSubmissionDrmContext.set(1);
    drm->receivedContextCreateFlags = 0;
    isDirectSubmissionRequested = false;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(ioctlVal, drm->receivedContextCreateFlags);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenQueryDistancesThenCorrectDistanceSet) {
    auto ioctlHelper = drm->getIoctlHelper();
    std::vector<DistanceInfo> distances(3);
    distances[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 0};
    distances[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    distances[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 1};
    distances[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    distances[2].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 4};
    distances[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2};
    std::vector<QueryItem> queryItems(distances.size());
    auto ret = ioctlHelper->queryDistances(queryItems, distances);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, distances[0].distance);
    EXPECT_EQ(0, distances[1].distance);
    EXPECT_EQ(100, distances[2].distance);

    EXPECT_EQ(0u, queryItems[0].dataPtr);
    EXPECT_EQ(0u, queryItems[1].dataPtr);
    EXPECT_EQ(0u, queryItems[2].dataPtr);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoWithDeviceMemoryThenDistancesUsedAndMultileValuesSet) {
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->memoryInfoQueried = true;
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto engineInfo = drm->getEngineInfo();

    auto &multiTileArchInfo = const_cast<GT_MULTI_TILE_ARCH_INFO &>(hwInfo->gtSystemInfo.MultiTileArchInfo);
    EXPECT_TRUE(multiTileArchInfo.IsValid);
    EXPECT_EQ(3, multiTileArchInfo.TileCount);
    EXPECT_EQ(7, multiTileArchInfo.TileMask);

    EXPECT_EQ(1024u, drm->memoryInfo->getMemoryRegionSize(1));
    EXPECT_EQ(1024u, drm->memoryInfo->getMemoryRegionSize(2));
    EXPECT_ANY_THROW(drm->memoryInfo->getMemoryRegionSize(4));

    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0u, engines);
    EXPECT_EQ(3u, engines.size());

    engines.clear();
    engineInfo->getListOfEnginesOnATile(1u, engines);
    EXPECT_EQ(3u, engines.size());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoThenCorrectCCSFlagsSet) {
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->memoryInfoQueried = true;
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto ccsInfo = hwInfo->gtSystemInfo.CCSInfo;
    EXPECT_TRUE(ccsInfo.IsValid);
    EXPECT_EQ_VAL(1u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ_VAL(1u, ccsInfo.Instances.CCSEnableMask);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenSysmanQueryEngineInfoThenAdditionalEnginesUsed) {
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    EXPECT_TRUE(drm->sysmanQueryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0u, engines);
    EXPECT_EQ(5u, engines.size());

    engines.clear();
    engineInfo->getListOfEnginesOnATile(1u, engines);
    EXPECT_EQ(5u, engines.size());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoAndFailIoctlThenFalseReturned) {
    drm->ioctlCallsCount = 0;
    drm->queryDistanceIoctlRetVal = -1;

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->memoryInfoQueried = true;
    EXPECT_FALSE(drm->queryEngineInfo());

    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    EXPECT_EQ(nullptr, engineInfo);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlFailureWhenCreateContextWithAccessCountersIsCalledThenErrorIsReturned) {
    drm->ioctlRetVal = EINVAL;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_THROW(ioctlHelper->createContextWithAccessCounters(gcc), std::runtime_error);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlSuccessWhenCreateContextWithAccessCountersIsCalledThenSuccessIsReturned) {
    drm->ioctlRetVal = 0;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_EQ(0u, ioctlHelper->createContextWithAccessCounters(gcc));
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlFailureWhenCreateCooperativeContexIsCalledThenErrorIsReturned) {
    drm->ioctlRetVal = EINVAL;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_THROW(ioctlHelper->createCooperativeContext(gcc), std::runtime_error);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlSuccessWhenCreateCooperativeContexIsCalledThenSuccessIsReturned) {
    drm->ioctlRetVal = 0u;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_EQ(0u, ioctlHelper->createCooperativeContext(gcc));
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, whenCreateDrmContextIsCalledThenIoctlIsCalledOnlyOnce) {
    drm->ioctlRetVal = 0u;

    DebugManagerStateRestore stateRestore;
    constexpr bool isCooperativeContextRequested = true;
    constexpr bool isDirectSubmissionRequested = false;

    for (auto &cooperativeContextRequested : {-1, 0, 1}) {
        debugManager.flags.ForceRunAloneContext.set(cooperativeContextRequested);
        for (auto &accessCountersRequested : {-1, 0, 1}) {
            debugManager.flags.CreateContextWithAccessCounters.set(accessCountersRequested);
            for (auto vmId = 0u; vmId < 3; vmId++) {
                drm->ioctlCallsCount = 0u;
                drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);

                EXPECT_EQ(vmId > 0 ? 2u : 1u, drm->ioctlCallsCount);
            }
        }
    }
}

TEST_F(IoctlHelperPrelimFixture, givenProgramDebuggingAndContextDebugSupportedWhenCreatingContextThenCooperativeFlagIsPassedToCreateDrmContextOnlyIfCCSEnginesArePresent) {
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    drm->contextDebugSupported = true;
    drm->callBaseCreateDrmContext = false;

    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = defaultHwInfo->platform.eProductFamily;

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext.ensureContextInitialized(false);

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    if (executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 0) {
        EXPECT_TRUE(drm->capturedCooperativeContextRequest);
    } else {
    }

    OsContextLinux osContext2(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::cooperative}));
    osContext2.ensureContextInitialized(false);

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_TRUE(drm->capturedCooperativeContextRequest);
}

TEST_F(IoctlHelperPrelimFixture, givenProgramDebuggingModeAndContextDebugSupportedAndRegularEngineUsageWhenCreatingContextThenCooperativeFlagIsNotPassedInOfflineDebuggingMode) {
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    drm->contextDebugSupported = true;
    drm->callBaseCreateDrmContext = false;

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext.ensureContextInitialized(false);

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);

    if (executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 0) {
        EXPECT_TRUE(drm->capturedCooperativeContextRequest);
    } else {
        EXPECT_FALSE(drm->capturedCooperativeContextRequest);
    }

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::offline);

    OsContextLinux osContext2(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext2.ensureContextInitialized(false);

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_FALSE(drm->capturedCooperativeContextRequest);

    OsContextLinux osContext3(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::cooperative}));
    osContext3.ensureContextInitialized(false);

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_TRUE(drm->capturedCooperativeContextRequest);
}

TEST(IoctlHelperPrelimTest, givenProgramDebuggingAndContextDebugSupportedWhenInitializingContextThenVmIsCreatedWithAllNecessaryFlags) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);

    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*drm);
    drm->contextDebugSupported = true;
    drm->callBaseCreateDrmContext = false;
    drm->bindAvailable = true;
    drm->setPerContextVMRequired(true);

    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = defaultHwInfo->platform.eProductFamily;

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext.ensureContextInitialized(false);

    EXPECT_FALSE(drm->receivedGemVmControl.flags & PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH);
    EXPECT_TRUE(drm->receivedGemVmControl.flags & PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND);
    if (drm->hasPageFaultSupport()) {
        EXPECT_TRUE(drm->receivedGemVmControl.flags & PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT);
    } else {
        EXPECT_FALSE(drm->receivedGemVmControl.flags & PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT);
    }
    EXPECT_EQ(1, drm->createDrmVmCalled);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlHelperWhenInitializatedThenIpVersionIsSet) {
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (productHelper.isPlatformQuerySupported() == false) {
        GTEST_SKIP();
    }

    auto &ipVersion = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->ipVersion;
    ipVersion = {};
    drm->ioctlHelper->setupIpVersion();
    EXPECT_EQ(ipVersion.revision, 1u);
    EXPECT_EQ(ipVersion.release, 2u);
    EXPECT_EQ(ipVersion.architecture, 3u);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlHelperAndPlatformQueryNotSupportedWhenSetupIpVersionThenIpVersionIsSetFromHelper) {
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (productHelper.isPlatformQuerySupported() == true) {
        GTEST_SKIP();
    }

    auto &compilerProductHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto &ipVersion = hwInfo->ipVersion;
    ipVersion = {};
    drm->ioctlHelper->setupIpVersion();
    auto config = compilerProductHelper.getHwIpVersion(*hwInfo);
    EXPECT_EQ(config, ipVersion.value);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlHelperWhenFailOnInitializationThenIpVersionIsCorrect) {
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    auto &compilerProductHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto &ipVersion = hwInfo->ipVersion;
    ipVersion = {};
    drm->failRetHwIpVersion = true;
    drm->ioctlHelper->setupIpVersion();
    auto config = compilerProductHelper.getHwIpVersion(*hwInfo);
    EXPECT_EQ(config, ipVersion.value);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlHelperWhenInvalidHwIpVersionSizeOnInitializationThenErrorIsPrinted) {

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (productHelper.isPlatformQuerySupported() == false) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(true);

    testing::internal::CaptureStderr();
    drm->returnInvalidHwIpVersionLength = true;
    drm->ioctlHelper->setupIpVersion();

    debugManager.flags.PrintDebugMessages.set(false);
    std::string output = testing::internal::GetCapturedStderr();
    std::string expectedOutput = "Size got from PRELIM_DRM_I915_QUERY_HW_IP_VERSION query does not match PrelimI915::prelim_drm_i915_query_hw_ip_version size\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.c_str());
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlHelperWhenFailOnInitializationAndPlatformQueryIsSupportedThenErrorIsPrinted) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(true);

    testing::internal::CaptureStderr();
    drm->failRetHwIpVersion = true;
    drm->ioctlHelper->setupIpVersion();

    debugManager.flags.PrintDebugMessages.set(false);
    std::string output = testing::internal::GetCapturedStderr();

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (productHelper.isPlatformQuerySupported()) {
        EXPECT_STRNE(output.c_str(), "");
    } else {
        EXPECT_STREQ(output.c_str(), "");
    }
}
