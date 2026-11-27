/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_buffer_object.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"

#include "clos_matchers.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <vector>

namespace NEO {
namespace SysCalls {
extern bool failMmap;
} // namespace SysCalls
} // namespace NEO

namespace {
void *capturedHostMmapAddr = nullptr;
size_t capturedHostMmapLen = 0u;
off_t capturedHostMmapOffset = 0;
int capturedHostMmapProt = 0;
int capturedHostMmapFlags = 0;
void *capturingHostMmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) throw() {
    capturedHostMmapAddr = addr;
    capturedHostMmapLen = len;
    capturedHostMmapOffset = offset;
    capturedHostMmapProt = prot;
    capturedHostMmapFlags = flags;
    return addr;
}
void *capturedNoReplaceAddress = nullptr;
size_t capturedNoReplaceSize = 0u;
uint32_t capturedNoReplaceCalls = 0u;
bool failCapturedNoReplace = false;
void *capturingMmapFixedNoReplace(void *address, size_t size) noexcept {
    capturedNoReplaceAddress = address;
    capturedNoReplaceSize = size;
    capturedNoReplaceCalls++;
    return failCapturedNoReplace ? MAP_FAILED : address;
}
void resetCapturedNoReplace() {
    capturedNoReplaceAddress = nullptr;
    capturedNoReplaceSize = 0u;
    capturedNoReplaceCalls = 0u;
    failCapturedNoReplace = false;
}
} // namespace

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemMemAdviseIsCalledThenAdviceSentToIoctlHelper, IsPVC) {

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    TestedDrmMemoryManager memoryManager(*executionEnvironment);

    auto device0 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const StackVec<uint32_t, 2> &vmIds, uint32_t numSubDevices) override {
            setVmSharedSystemMemAdviseCalled++;
            numSubDevicesCalled = numSubDevices;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
        uint32_t numSubDevicesCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    std::vector<MemoryRegion> regionInfo{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};

    auto &drm0 = static_cast<DrmMockCustom &>(memoryManager.getDrm(device0->getRootDeviceIndex()));
    auto &drm1 = static_cast<DrmMockCustom &>(memoryManager.getDrm(device1->getRootDeviceIndex()));

    drm0.memoryInfo.reset(new MemoryInfo(regionInfo, drm0));
    drm1.memoryInfo.reset(new MemoryInfo(regionInfo, drm1));
    drm0.ioctlHelper.reset(mockIoctlHelper);
    drm0.ioctlExpected.total = -1;
    drm1.ioctlExpected.total = -1;

    MemAdvise memAdviseOp = MemAdvise::setPreferredLocation;
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, *device0.get(), *device1.get()));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(2u, mockIoctlHelper->numSubDevicesCalled);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemMemAdviseIsCalledWithIncorrectNumberSubdevicesThenAdviceNotSentToIoctlHelper, IsPVC) {

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(4);

    TestedDrmMemoryManager memoryManager(*executionEnvironment);

    auto device0 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const StackVec<uint32_t, 2> &vmIds, uint32_t numSubDevices) override {
            setVmSharedSystemMemAdviseCalled++;
            numSubDevicesCalled = numSubDevices;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
        uint32_t numSubDevicesCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    std::vector<MemoryRegion> regionInfo{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};

    auto &drm0 = static_cast<DrmMockCustom &>(memoryManager.getDrm(device0->getRootDeviceIndex()));
    auto &drm1 = static_cast<DrmMockCustom &>(memoryManager.getDrm(device1->getRootDeviceIndex()));

    drm0.memoryInfo.reset(new MemoryInfo(regionInfo, drm0));
    drm1.memoryInfo.reset(new MemoryInfo(regionInfo, drm1));
    drm0.ioctlHelper.reset(mockIoctlHelper);
    drm0.ioctlExpected.total = -1;
    drm1.ioctlExpected.total = -1;

    MemAdvise memAdviseOp = MemAdvise::setPreferredLocation;
    EXPECT_FALSE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, *device0.get(), *device1.get()));
    EXPECT_EQ(0u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemAtomicAccessIsCalledForSingleDeviceThenAdviceSentToIoctlHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const StackVec<uint32_t, 2> &vmIds, uint32_t numSubDevices) override {
            setVmSharedSystemMemAdviseCalled++;
            numSubDevicesCalled = numSubDevices;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
        uint32_t numSubDevicesCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager.getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto subDeviceIds = NEO::SubDeviceIdsVec{0};
    AtomicAccessMode mode = AtomicAccessMode::device;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(1u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(2u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(1u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::host;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(3u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(1u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::none;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(4u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(1u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::invalid;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(5u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(1u, mockIoctlHelper->numSubDevicesCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemAtomicAccessIsCalledForTwoSubdevicesThenAdviceSentToIoctlHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const StackVec<uint32_t, 2> &vmIds, uint32_t numSubDevices) override {
            setVmSharedSystemMemAdviseCalled++;
            numSubDevicesCalled = numSubDevices;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
        uint32_t numSubDevicesCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager.getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto subDeviceIds = NEO::SubDeviceIdsVec{0, 1};
    AtomicAccessMode mode = AtomicAccessMode::device;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(2u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(2u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(2u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::host;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(3u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(2u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::none;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(4u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(2u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::invalid;
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(5u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(2u, mockIoctlHelper->numSubDevicesCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemAtomicAccessIsCalledForIncorrectNumberSubdevicesThenAdviceNotSentToIoctlHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const StackVec<uint32_t, 2> &vmIds, uint32_t numSubDevices) override {
            setVmSharedSystemMemAdviseCalled++;
            numSubDevicesCalled = numSubDevices;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
        uint32_t numSubDevicesCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager.getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto subDeviceIds = NEO::SubDeviceIdsVec{0, 1, 2, 3};
    AtomicAccessMode mode = AtomicAccessMode::device;
    EXPECT_FALSE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(0u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(0u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::system;
    EXPECT_FALSE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(0u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(0u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::host;
    EXPECT_FALSE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(0u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(0u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::none;
    EXPECT_FALSE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(0u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(0u, mockIoctlHelper->numSubDevicesCalled);

    mode = AtomicAccessMode::invalid;
    EXPECT_FALSE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, mode, subDeviceIds, 0u));
    EXPECT_EQ(0u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(0u, mockIoctlHelper->numSubDevicesCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenGetSharedSystemAtomicAccessIsCalledThenAdviceReturnedToIoctlHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        AtomicAccessMode getVmSharedSystemAtomicAttribute(uint64_t handle, const size_t size, uint32_t vmIds) override {
            getVmSharedSystemAtomicAttributeCalled++;
            return mode;
        }
        uint32_t getVmSharedSystemAtomicAttributeCalled = 0;
        AtomicAccessMode mode = AtomicAccessMode::none;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager.getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    auto subDeviceIds = NEO::SubDeviceIdsVec{0};
    EXPECT_EQ(AtomicAccessMode::none, memoryManager.getSharedSystemAtomicAccess(nullptr, 0u, subDeviceIds, 0u));
    EXPECT_EQ(1u, mockIoctlHelper->getVmSharedSystemAtomicAttributeCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetAtomicAccessIsCalledThenTrueReturned) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unifiedSharedMemory, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    size_t size = 16;
    AtomicAccessMode mode = AtomicAccessMode::none;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));

    mode = AtomicAccessMode::device;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));

    mode = AtomicAccessMode::system;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));

    mode = AtomicAccessMode::host;
    EXPECT_TRUE(memoryManager.setAtomicAccess(&drmAllocation, size, mode, rootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithSubDevicesThenPrefetchBOsToTheseSubDevices) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0, 1};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(4u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(4u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[1]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[2]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[2]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[3]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[3]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithASubDeviceThenPrefetchBOsToTheSubDevices) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(2u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(2u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[1]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKMDSupportForCrossTileMigrationPolicyAndKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithASubDeviceThenPrefetchBOsToThisSubDevice) {
    DebugManagerStateRestore restore;
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(2u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(2u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[1]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKMDSupportForCrossTileMigrationPolicyAndKmdMigratedSharedAllocationWithMultipleBOsWhenSetMemPrefetchIsCalledWithSubDevicesThenPrefetchBOsToTheseSubDevices) {
    DebugManagerStateRestore restore;
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0, 1};
    BufferObject bo0(rootDeviceIndex, mock, 3, 1, 1024, 0);
    BufferObject bo1(rootDeviceIndex, mock, 3, 2, 1024, 0);
    BufferObjects bos{&bo0, &bo1};

    MockDrmAllocation drmAllocation(AllocationType::unifiedSharedMemory, MemoryPool::localMemory, bos);
    drmAllocation.storageInfo.memoryBanks = 0x3;
    drmAllocation.setNumHandles(2);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(4u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(4u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[1]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[1]);

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[2]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[2]);

    EXPECT_EQ(1u, drmAllocation.vmHandleIdsReceived[3]);
    EXPECT_EQ(1u, drmAllocation.subDeviceIdsReceived[3]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMigratedSharedAllocationWithSingleBOWhenSetMemPrefetchIsCalledWithASubDeviceThenPrefetchBOToThisSubdevice) {
    const uint32_t rootDeviceIndex = this->device->getRootDeviceIndex();
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    MockDrmAllocation drmAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    drmAllocation.bufferObjects[0] = &bo;
    drmAllocation.setNumHandles(1);

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_TRUE(drmAllocation.prefetchBOCalled);

    ASSERT_EQ(1u, drmAllocation.vmHandleIdsReceived.size());
    ASSERT_EQ(1u, drmAllocation.subDeviceIdsReceived.size());

    EXPECT_EQ(0u, drmAllocation.vmHandleIdsReceived[0]);
    EXPECT_EQ(0u, drmAllocation.subDeviceIdsReceived[0]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemPrefetchFailsToBindBufferObjectThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    SubDeviceIdsVec subDeviceIds{0};
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    MockDrmAllocation drmAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    drmAllocation.bufferObjects[0] = &bo;

    memoryManager.allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager.getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }

    drmAllocation.bindBOsRetValue = -1;

    EXPECT_FALSE(memoryManager.setMemPrefetch(&drmAllocation, subDeviceIds, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
    EXPECT_FALSE(drmAllocation.prefetchBOCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPrefetchSharedSystemAllocIsCalledThenReturnTrue) {

    uint8_t data{};

    auto subDeviceIds = NEO::SubDeviceIdsVec{0};
    EXPECT_TRUE(memoryManager->prefetchSharedSystemAlloc(&data, sizeof(data), subDeviceIds, mockRootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPrefetchSharedSystemAllocIsCalledThenReturnFalse) {

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override {
            setVmSharedSystemMemPrefetchCalled++;
            return false;
        }
        uint32_t setVmSharedSystemMemPrefetchCalled = 0;
    };
    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    uint8_t data{};

    auto subDeviceIds = NEO::SubDeviceIdsVec{0};
    EXPECT_FALSE(memoryManager->prefetchSharedSystemAlloc(&data, sizeof(data), subDeviceIds, mockRootDeviceIndex));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemPrefetchCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPageFaultIsUnSupportedWhenCallingBindBoOnBufferAllocationThenAllocationShouldNotPageFaultAndExplicitResidencyIsNotRequired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drm.pageFaultSupported);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    std::vector<BufferObject *> bufferObjects;
    allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true, false);

    EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
    EXPECT_FALSE(bo.isExplicitResidencyRequired());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPageFaultIsSupportedAndKmdMigrationEnabledForBuffersWhenCallingBindBoOnBufferAllocationThenAllocationShouldPageFaultAndExplicitResidencyIsNotRequired) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableRecoverablePageFaults.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drm.pageFaultSupported);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    for (auto useKmdMigrationForBuffers : {-1, 0, 1}) {
        debugManager.flags.UseKmdMigrationForBuffers.set(useKmdMigrationForBuffers);

        std::vector<BufferObject *> bufferObjects;
        allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true, false);

        if (useKmdMigrationForBuffers > 0) {
            EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
            EXPECT_FALSE(bo.isExplicitResidencyRequired());
        } else {
            EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
            EXPECT_TRUE(bo.isExplicitResidencyRequired());
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPageFaultIsSupportedWhenCallingBindBoOnAllocationThatShouldPageFaultThenExplicitResidencyIsNotRequired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.pageFaultSupported = true;

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    struct MockDrmAllocationToTestPageFault : MockDrmAllocation {
        MockDrmAllocationToTestPageFault() : MockDrmAllocation(0u, AllocationType::buffer, MemoryPool::localMemory) {};
        bool shouldAllocationPageFault(const Drm *drm) override {
            return shouldPageFault;
        }
        bool shouldPageFault = false;
    };

    for (auto shouldAllocationPageFault : {false, true}) {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocationToTestPageFault allocation;
        allocation.bufferObjects[0] = &bo;
        allocation.shouldPageFault = shouldAllocationPageFault;

        std::vector<BufferObject *> bufferObjects;
        allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true, false);

        EXPECT_EQ(shouldAllocationPageFault, allocation.shouldAllocationPageFault(&drm));
        EXPECT_EQ(!shouldAllocationPageFault, bo.isExplicitResidencyRequired());
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor2ChunksThenActualValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(2);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = MemoryConstants::megaByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor3ChunksThenCorrectedValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(3);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = MemoryConstants::megaByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor6ChunksThenCorrectedValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(6);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = 512 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkForUnevenChunksThenCorrectedValueReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(2);
    size_t allocSize = 2 * MemoryConstants::megaByte + 64 * MemoryConstants::kiloByte;
    size_t expectedSize = 704 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkFor1ChunkThenDefaultMinimumChunkSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(1);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = 64 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenGetSizeOfChunkForTooManyChunksThenDefaultMinimumChunkSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.NumberOfBOChunks.set(10000);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = 64 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeThenSameSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(64 * MemoryConstants::kiloByte);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = 64 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeThenCorrectedSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(100000);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = 64 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeTooLargeThenCorrectedSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(4000000);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = MemoryConstants::megaByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenSetChunkSizeTooSmallThenCorrectedSizeReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetBOChunkingSize.set(4000);
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t expectedSize = 64 * MemoryConstants::kiloByte;
    size_t chunkSize = memoryManager->getSizeOfChunk(allocSize);
    EXPECT_EQ(expectedSize, chunkSize);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingReturnTrue) {
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t minSize = 2 * MemoryConstants::megaByte;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_TRUE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    minSize = MemoryConstants::megaByte;
    EXPECT_TRUE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithImproperAllocSizeReturnFalse) {
    size_t allocSize = 2098000;
    size_t minSize = 2 * MemoryConstants::megaByte;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithUnevenNumberChunksReturnsFalse) {
    size_t allocSize = 2 * MemoryConstants::megaByte + 64 * MemoryConstants::kiloByte;
    size_t minSize = 2 * MemoryConstants::megaByte;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithAllocationLessThanMinSizeReturnsFalse) {
    size_t allocSize = 100000;
    size_t minSize = 2 * MemoryConstants::megaByte;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCheckAllocationForChunkingWithBooleanInputFalseReturnsFalse) {
    size_t allocSize = 2 * MemoryConstants::megaByte;
    size_t minSize = 2 * MemoryConstants::megaByte;
    bool subDeviceEnabled = true;
    bool debugDisabled = true;
    bool modeEnabled = true;
    bool bufferEnabled = true;
    EXPECT_TRUE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    subDeviceEnabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    subDeviceEnabled = true;
    debugDisabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    debugDisabled = true;
    modeEnabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
    modeEnabled = true;
    bufferEnabled = false;
    EXPECT_FALSE(memoryManager->checkAllocationForChunking(allocSize, minSize, subDeviceEnabled, debugDisabled, modeEnabled, bufferEnabled));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithoutMemoryInfoThenNullBufferObjectIsReturned) {
    mock->memoryInfo.reset(nullptr);

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(rootDeviceIndex, nullptr, AllocationType::buffer, gpuAddress, size, MemoryBanks::mainBank, 1, -1, false, false, false));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithZeroSizeThenNullBufferObjectIsReturned) {
    auto gpuAddress = 0x1234u;
    auto size = 0u;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(rootDeviceIndex, nullptr, AllocationType::buffer, gpuAddress, size, MemoryBanks::mainBank, 1, -1, false, false, false));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseKmdMigrationForBuffersWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferWithSeveralMemoryBanksThenCreateGemObjectWithMultipleRegions) {
    debugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->banks);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseKmdMigrationForBuffersWhenGraphicsAllocationInPhysicalLocalMemoryIsAllocatedForBufferWithSeveralMemoryBanksThenCreateGemObjectWithMultipleRegions) {
    debugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(0u, allocation->getGpuAddress());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->banks);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUAllocationInPhysicalLocalMemoryIsAllocatedWithNoCacheRegionThenFailureReturned) {
    debugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.cacheRegion = 0xFFFF;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(0));
    drm.cacheInfo.reset(nullptr);

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithNoSetPairAndOneHandleAndCommandBufferTypeThenNoPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::commandBuffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalLocalMemoryAllocationThenSuccessAndNoVirtualMemoryAssigned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::commandBuffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithSetPairAndOneHandleThenThenNoPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithSetPairAndTwoHandlesThenPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_NE(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithLessThanChunkingSizeAllowedThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::chunkThreshold / 4;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(false, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->isChunkedUsed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithMoreThanChunkingSizeAllowedThenChunkingIsUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x3};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(true, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->isChunkedUsed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToSharedThenNoChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x1};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceAndOnlyOneTileThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceThenChunkingIsUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceAndChunkingSizeNotAlignedThenChunkingIsNotUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.NumberOfBOChunks.set(64);

    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenDeviceMemoryAllocationWithChunkingModeSetToDeviceAndSizeLessThanMinimalThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x2};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->storageInfo.isChunked);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithMoreThanChunkingSizeAllowedWithDebuggingEnabledThenChunkingIsNotUsed) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x3};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    executionEnvironment->setDebuggingMode(DebuggingMode::offline);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(false, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->isChunkedUsed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest,
       givenMemoryAllocationWithMoreThanChunkingSizeAllowedAndFailGemCreateExtThenNullptrIsReturned) {
    VariableBackup<bool> backupChunkingCallParent{&mock->getChunkingAvailableCall.callParent, false};
    VariableBackup<bool> backupChunkingReturnValue{&mock->getChunkingAvailableCall.returnValue, true};
    VariableBackup<bool> backupChunkingModeCallParent{&mock->getChunkingModeCall.callParent, false};
    VariableBackup<uint32_t> backupChunkingModeReturnValue{&mock->getChunkingModeCall.returnValue, 0x3};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;
    allocData.storageInfo.subDeviceBitfield = 0b11;
    static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->failOnCreateGemExtWithMultipleRegions = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithNoSetPairAndTwoHandlesThenPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, false};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseSystemMemoryFlagWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSvmGpuAllocationWhenHostPtrProvidedThenUseHostPtrAsGpuVa) {
    size_t size = 2 * MemoryConstants::megaByte;
    AllocationProperties properties{rootDeviceIndex, false, size, AllocationType::svmGpu, false, mockDeviceBitfield};
    properties.alignment = size;
    void *svmPtr = reinterpret_cast<void *>(2 * size);

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, svmPtr));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

    EXPECT_EQ(svmPtr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    EXPECT_EQ(0u, allocation->getReservedAddressSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithSameGpuVirtualAddressAndSize) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 3 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};
    for (uint32_t i = 0; i < 2; i++) {
        allocData.type = isaTypes[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
        EXPECT_NE(0u, allocation->getGpuAddress());
        EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

        auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
        auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
        auto expectedSize = alignUp(allocData.size, alignmentSize);

        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto &bos = drmAllocation->getBOs();
        auto boAddress = drmAllocation->getGpuAddress();
        for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
            auto bo = bos[handleId];
            ASSERT_NE(nullptr, bo);
            auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
            EXPECT_EQ(boAddress, bo->peekAddress());
            EXPECT_EQ(boSize, bo->peekSize());
            EXPECT_EQ(expectedSize, boSize);
        }

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithLargeBufferWhenAllocatingInLocalPhysicalMemoryOnAllMemoryBanksThenCreateFourBufferObjectsWithContiguousGpuVirtualAddressesAndPartialSizes) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    uint64_t gpuAddress = 0x1234;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());
    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, 0u));
    EXPECT_EQ(0u, allocation->getGpuAddress());

    auto gmmHelper = device->getGmmHelper();
    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    EXPECT_EQ(gmmHelper->canonize(gpuAddress), mappedAllocation->getGpuAddress());

    auto &bos = mappedAllocation->getBOs();
    auto boAddress = gpuAddress;
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(boSize, bo->peekSize());
        EXPECT_EQ(boSize, handleId == 0 || handleId == 1 ? 5 * MemoryConstants::pageSize64k : 4 * MemoryConstants::pageSize64k);
        boAddress += boSize;
    }
    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, rootDeviceIndex));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(0u, allocation->getGpuAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocationInLocalPhysicalMemoryAndDeviceBitfieldWithHolesThenCorrectAllocationCreated) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = 0b1011;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto kernelIsaAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));

    EXPECT_NE(nullptr, kernelIsaAllocation);

    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, kernelIsaAllocation, gpuAddress, allocData.size, nullptr, 0u));

    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    auto gpuAddressReserved = mappedAllocation->getGpuAddress();
    auto &bos = mappedAllocation->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(gpuAddressReserved, bos[0]->peekAddress());
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_EQ(gpuAddressReserved, bos[1]->peekAddress());

    EXPECT_EQ(nullptr, bos[2]);

    EXPECT_NE(nullptr, bos[3]);
    EXPECT_EQ(gpuAddressReserved, bos[3]->peekAddress());

    auto &storageInfo = mappedAllocation->storageInfo;
    EXPECT_EQ(0b1011u, storageInfo.memoryBanks.to_ulong());

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, rootDeviceIndex));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    memoryManager->freeGraphicsMemory(kernelIsaAllocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenVmBindAvailabilityWhenQueryingSupportsPhysicalDeviceMemoryMultiMappingThenAvailabilityIsReturned) {
    mock->isVmBindAvailableCall.callParent = false;

    mock->isVmBindAvailableCall.returnValue = true;
    EXPECT_TRUE(memoryManager->supportsPhysicalDeviceMemoryMultiMapping(rootDeviceIndex));

    mock->isVmBindAvailableCall.returnValue = false;
    EXPECT_FALSE(memoryManager->supportsPhysicalDeviceMemoryMultiMapping(rootDeviceIndex));
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryWhenMappedToTwoVirtualAddressesThenBothMappingsCoexistAndShareThePhysicalHandle) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 8 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    auto physicalBo = allocation->getBOs()[0];
    ASSERT_NE(nullptr, physicalBo);
    auto physicalHandle = physicalBo->peekHandle();
    auto baseAddress = physicalBo->peekAddress();
    auto gmmHelper = device->getGmmHelper();

    uint64_t gpuAddress0 = 0x100000;
    uint64_t gpuAddress1 = 0x200000;

    MultiGraphicsAllocation mappingA(rootDeviceIndex);
    MultiGraphicsAllocation mappingB(rootDeviceIndex);

    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingA, allocation, gpuAddress0, allocData.size, nullptr, 0u));
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingB, allocation, gpuAddress1, allocData.size, nullptr, 0u));

    auto mappedA = static_cast<DrmAllocation *>(mappingA.getGraphicsAllocation(rootDeviceIndex));
    auto mappedB = static_cast<DrmAllocation *>(mappingB.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedA);
    ASSERT_NE(nullptr, mappedB);
    EXPECT_NE(mappedA, mappedB);
    EXPECT_NE(mappedA, static_cast<DrmAllocation *>(allocation));

    auto boA = mappedA->getBOs()[0];
    auto boB = mappedB->getBOs()[0];
    ASSERT_NE(nullptr, boA);
    ASSERT_NE(nullptr, boB);
    EXPECT_NE(boA, boB);

    EXPECT_EQ(gmmHelper->canonize(gpuAddress0 + baseAddress), boA->peekAddress());
    EXPECT_EQ(gmmHelper->canonize(gpuAddress1 + baseAddress), boB->peekAddress());
    EXPECT_EQ(physicalHandle, boA->peekHandle());
    EXPECT_EQ(physicalHandle, boB->peekHandle());

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingA, mappedA, gpuAddress0, allocData.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingB, mappedB, gpuAddress1, allocData.size, osContext, rootDeviceIndex));
    EXPECT_EQ(nullptr, mappingA.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(nullptr, mappingB.getGraphicsAllocation(rootDeviceIndex));

    EXPECT_EQ(baseAddress, physicalBo->peekAddress());
    EXPECT_EQ(physicalHandle, physicalBo->peekHandle());

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryMappingWhenUnmappedThenParkedBufferObjectIsMovedOffTheVirtualAddressUntilMappedAgain) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    const auto baseAddress = allocation->getBOs()[0]->peekAddress();
    const uint64_t gpuRange = 0x100000;
    auto osContext = device->getDefaultEngine().osContext;

    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuRange, allocData.size, nullptr, 0u));
    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    auto mappingBo = mappedAllocation->getBOs()[0];
    EXPECT_EQ(device->getGmmHelper()->canonize(gpuRange + baseAddress), mappingBo->peekAddress());

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuRange, allocData.size, osContext, rootDeviceIndex));
    EXPECT_EQ(baseAddress, mappingBo->peekAddress());
    EXPECT_EQ(0u, mappingBo->getVirtualMappingSize());

    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuRange, allocData.size, nullptr, 0u));
    EXPECT_EQ(mappedAllocation, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(device->getGmmHelper()->canonize(gpuRange + baseAddress), mappingBo->peekAddress());
    EXPECT_EQ(allocData.size, mappingBo->getVirtualMappingSize());

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuRange, allocData.size, osContext, rootDeviceIndex));
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUnmappedPhysicalDeviceMemoryWhenMappedAtAnotherVirtualAddressThenParkedMappingIsPlacedThereWithAndWithoutVmBind) {
    mock->isVmBindAvailableCall.callParent = false;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    const uint64_t firstGpuRange = 0x100000;
    const uint64_t secondGpuRange = 0x200000;
    const size_t secondMappingSize = allocData.size / 2;
    auto osContext = device->getDefaultEngine().osContext;
    auto gmmHelper = device->getGmmHelper();

    for (const bool vmBindAvailable : {false, true}) {
        mock->isVmBindAvailableCall.returnValue = vmBindAvailable;
        auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
        ASSERT_NE(nullptr, allocation);
        const auto baseAddress = allocation->getBOs()[0]->peekAddress();

        MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
        ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, firstGpuRange, allocData.size, nullptr, 0u));
        auto firstMapping = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        ASSERT_NE(nullptr, firstMapping);
        EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, firstMapping, firstGpuRange, allocData.size, osContext, rootDeviceIndex));

        ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, secondGpuRange, secondMappingSize, nullptr, 0u));
        auto secondMapping = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        ASSERT_NE(nullptr, secondMapping);
        EXPECT_EQ(firstMapping, secondMapping);
        EXPECT_EQ(0u, allocation->getPhysicalMappingsInfo().parkedMappings.size());
        EXPECT_EQ(gmmHelper->canonize(secondGpuRange), secondMapping->getGpuAddress());
        EXPECT_EQ(secondMappingSize, secondMapping->getUnderlyingBufferSize());
        EXPECT_EQ(gmmHelper->canonize(secondGpuRange + baseAddress), secondMapping->getBOs()[0]->peekAddress());

        EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, secondMapping, secondGpuRange, secondMappingSize, osContext, rootDeviceIndex));
        memoryManager->freeGraphicsMemory(allocation);
    }
    mock->isVmBindAvailableCall.callParent = true;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenRegisterSysMemAllocFailingWhenMappingParkedMappingAtAnotherVirtualAddressThenMappingStaysParkedOffTheVirtualAddressWithAndWithoutVmBind) {
    mock->isVmBindAvailableCall.callParent = false;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    const uint64_t firstGpuRange = 0x100000;
    const uint64_t secondGpuRange = 0x200000;
    auto osContext = device->getDefaultEngine().osContext;

    for (const bool vmBindAvailable : {false, true}) {
        mock->isVmBindAvailableCall.returnValue = vmBindAvailable;
        auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalDeviceMemory(allocData, status));
        ASSERT_NE(nullptr, allocation);
        const auto baseAddress = allocation->getBOs()[0]->peekAddress();

        MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
        ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, firstGpuRange, allocData.size, nullptr, 0u));
        auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        ASSERT_NE(nullptr, mappedAllocation);
        EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, firstGpuRange, allocData.size, osContext, rootDeviceIndex));

        memoryManager->failRegisterSysMemAlloc = true;
        EXPECT_FALSE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, secondGpuRange, allocData.size, nullptr, 0u));
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        EXPECT_EQ(0u, allocation->getPhysicalMappingsInfo().liveMappingsCount);
        ASSERT_EQ(1u, allocation->getPhysicalMappingsInfo().parkedMappings.size());
        EXPECT_EQ(mappedAllocation, allocation->getPhysicalMappingsInfo().parkedMappings[0].mappedAllocation);
        EXPECT_EQ(secondGpuRange, allocation->getPhysicalMappingsInfo().parkedMappings[0].gpuRange);
        EXPECT_EQ(baseAddress, mappedAllocation->getBOs()[0]->peekAddress());
        EXPECT_EQ(0u, mappedAllocation->getBOs()[0]->getVirtualMappingSize());

        memoryManager->failRegisterSysMemAlloc = false;
        memoryManager->freeGraphicsMemory(allocation);
    }
    mock->isVmBindAvailableCall.callParent = true;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSeveralParkedPhysicalDeviceMemoryMappingsWhenMappingTheRangeOfOneAndThenANewRangeThenMatchingOneIsReusedFirstAndRemainingOneIsPlacedAtNewRange) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    const auto baseAddress = allocation->getBOs()[0]->peekAddress();
    const uint64_t firstGpuRange = 0x100000;
    const uint64_t secondGpuRange = 0x200000;
    const uint64_t thirdGpuRange = 0x300000;
    auto osContext = device->getDefaultEngine().osContext;
    auto gmmHelper = device->getGmmHelper();

    MultiGraphicsAllocation firstMultiGraphicsAllocation(rootDeviceIndex);
    MultiGraphicsAllocation secondMultiGraphicsAllocation(rootDeviceIndex);
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(firstMultiGraphicsAllocation, allocation, firstGpuRange, allocData.size, nullptr, 0u));
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(secondMultiGraphicsAllocation, allocation, secondGpuRange, allocData.size, nullptr, 0u));
    auto firstMapping = static_cast<DrmAllocation *>(firstMultiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    auto secondMapping = static_cast<DrmAllocation *>(secondMultiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, firstMapping);
    ASSERT_NE(nullptr, secondMapping);
    ASSERT_NE(firstMapping, secondMapping);
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(firstMultiGraphicsAllocation, firstMapping, firstGpuRange, allocData.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(secondMultiGraphicsAllocation, secondMapping, secondGpuRange, allocData.size, osContext, rootDeviceIndex));
    const auto &parkedMappings = allocation->getPhysicalMappingsInfo().parkedMappings;
    ASSERT_EQ(2u, parkedMappings.size());

    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(secondMultiGraphicsAllocation, allocation, secondGpuRange, allocData.size, nullptr, 0u));
    EXPECT_EQ(secondMapping, secondMultiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(gmmHelper->canonize(secondGpuRange + baseAddress), secondMapping->getBOs()[0]->peekAddress());
    ASSERT_EQ(1u, parkedMappings.size());
    EXPECT_EQ(firstMapping, parkedMappings[0].mappedAllocation);
    EXPECT_EQ(firstGpuRange, parkedMappings[0].gpuRange);

    MultiGraphicsAllocation thirdMultiGraphicsAllocation(rootDeviceIndex);
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(thirdMultiGraphicsAllocation, allocation, thirdGpuRange, allocData.size, nullptr, 0u));
    EXPECT_EQ(firstMapping, thirdMultiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(gmmHelper->canonize(thirdGpuRange), firstMapping->getGpuAddress());
    EXPECT_EQ(gmmHelper->canonize(thirdGpuRange + baseAddress), firstMapping->getBOs()[0]->peekAddress());
    EXPECT_EQ(gmmHelper->canonize(secondGpuRange + baseAddress), secondMapping->getBOs()[0]->peekAddress());
    EXPECT_TRUE(parkedMappings.empty());
    EXPECT_EQ(2u, allocation->getPhysicalMappingsInfo().liveMappingsCount);

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(secondMultiGraphicsAllocation, secondMapping, secondGpuRange, allocData.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(thirdMultiGraphicsAllocation, firstMapping, thirdGpuRange, allocData.size, osContext, rootDeviceIndex));
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryRepeatedlyMappedAndUnmappedAtNewVirtualAddressesThenOneMappingAndItsBufferObjectAreReusedWithAndWithoutVmBind) {
    mock->isVmBindAvailableCall.callParent = false;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto osContext = device->getDefaultEngine().osContext;
    auto gmmHelper = device->getGmmHelper();

    for (const bool vmBindAvailable : {false, true}) {
        mock->isVmBindAvailableCall.returnValue = vmBindAvailable;
        auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
        ASSERT_NE(nullptr, allocation);
        const auto baseAddress = allocation->getBOs()[0]->peekAddress();

        DrmAllocation *mapping = nullptr;
        BufferObject *mappingBo = nullptr;
        MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
        for (const uint64_t gpuRange : {0x100000ull, 0x200000ull, 0x300000ull, 0x400000ull}) {
            ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuRange, allocData.size, nullptr, 0u));
            auto currentMapping = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
            ASSERT_NE(nullptr, currentMapping);
            if (mapping == nullptr) {
                mapping = currentMapping;
                mappingBo = currentMapping->getBOs()[0];
            }
            EXPECT_EQ(mapping, currentMapping);
            EXPECT_EQ(mappingBo, currentMapping->getBOs()[0]);
            EXPECT_EQ(gmmHelper->canonize(gpuRange), currentMapping->getGpuAddress());
            EXPECT_EQ(gmmHelper->canonize(gpuRange + baseAddress), mappingBo->peekAddress());
            EXPECT_TRUE(allocation->getPhysicalMappingsInfo().parkedMappings.empty());

            EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, currentMapping, gpuRange, allocData.size, osContext, rootDeviceIndex));
            ASSERT_EQ(1u, allocation->getPhysicalMappingsInfo().parkedMappings.size());
            EXPECT_EQ(gpuRange, allocation->getPhysicalMappingsInfo().parkedMappings[0].gpuRange);
        }
        memoryManager->freeGraphicsMemory(allocation);
    }
    mock->isVmBindAvailableCall.callParent = true;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUnmappedPhysicalDeviceMemoryWhenMappedAtAnotherVirtualAddressWithOffsetThenReusedBufferObjectCarriesNewOffsetAndSizeUntilUnmapped) {
    mock->isVmBindAvailableCall.callParent = false;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 8 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    const uint64_t firstGpuRange = 0x100000;
    const uint64_t secondGpuRange = 0x400000;
    const size_t secondMappingSize = 2 * MemoryConstants::pageSize64k;
    const size_t secondOffset = 2 * MemoryConstants::pageSize;
    auto osContext = device->getDefaultEngine().osContext;
    auto gmmHelper = device->getGmmHelper();

    for (const bool vmBindAvailable : {false, true}) {
        mock->isVmBindAvailableCall.returnValue = vmBindAvailable;
        auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
        ASSERT_NE(nullptr, allocation);
        auto physicalBo = allocation->getBOs()[0];
        const auto baseAddress = physicalBo->peekAddress();

        MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
        ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, firstGpuRange, allocData.size, nullptr, 0u));
        auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        ASSERT_NE(nullptr, mappedAllocation);
        auto mappingBo = mappedAllocation->getBOs()[0];
        EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, firstGpuRange, allocData.size, osContext, rootDeviceIndex));

        ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, secondGpuRange, secondMappingSize, nullptr, secondOffset));
        EXPECT_EQ(mappedAllocation, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
        EXPECT_EQ(mappingBo, mappedAllocation->getBOs()[0]);
        const auto expectedBoAddress = vmBindAvailable ? secondGpuRange + baseAddress : secondGpuRange + baseAddress - secondOffset;
        EXPECT_EQ(gmmHelper->canonize(expectedBoAddress), mappingBo->peekAddress());
        EXPECT_EQ(secondOffset, mappingBo->getPhysicalMemoryOffset());
        EXPECT_EQ(secondMappingSize, mappingBo->getVirtualMappingSize());
        EXPECT_EQ(gmmHelper->canonize(secondGpuRange), mappedAllocation->getGpuAddress());
        EXPECT_EQ(secondMappingSize, mappedAllocation->getUnderlyingBufferSize());
        EXPECT_EQ(baseAddress, physicalBo->peekAddress());
        EXPECT_EQ(0u, physicalBo->getPhysicalMemoryOffset());
        EXPECT_EQ(0u, physicalBo->getVirtualMappingSize());

        EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, secondGpuRange, secondMappingSize, osContext, rootDeviceIndex));
        EXPECT_EQ(baseAddress, mappingBo->peekAddress());
        EXPECT_EQ(0u, mappingBo->getPhysicalMemoryOffset());
        EXPECT_EQ(0u, mappingBo->getVirtualMappingSize());
        memoryManager->freeGraphicsMemory(allocation);
    }
    mock->isVmBindAvailableCall.callParent = true;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryMappedToVirtualAddressesThenMappingsAreNotCountedInUsedLocalMemory) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 8 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    auto usedBeforeMap = memoryManager->getUsedLocalMemorySize(rootDeviceIndex);

    MultiGraphicsAllocation mappingA(rootDeviceIndex);
    MultiGraphicsAllocation mappingB(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingA, allocation, 0x100000, allocData.size, nullptr, 0u));
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingB, allocation, 0x200000, allocData.size, nullptr, 0u));

    EXPECT_EQ(usedBeforeMap, memoryManager->getUsedLocalMemorySize(rootDeviceIndex));

    auto mappedA = static_cast<DrmAllocation *>(mappingA.getGraphicsAllocation(rootDeviceIndex));
    auto mappedB = static_cast<DrmAllocation *>(mappingB.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_TRUE(mappedA->isSharedPhysicalMemoryMapping());
    EXPECT_TRUE(mappedB->isSharedPhysicalMemoryMapping());

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingA, mappedA, 0x100000, allocData.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingB, mappedB, 0x200000, allocData.size, osContext, rootDeviceIndex));

    EXPECT_EQ(usedBeforeMap, memoryManager->getUsedLocalMemorySize(rootDeviceIndex));

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryMappedToVirtualAddressesWhenMappingsAreUnmappedThenMemoryBankUsageIsUnchanged) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    AllocationProperties properties(rootDeviceIndex, true, 8 * MemoryConstants::pageSize64k, AllocationType::buffer, false, false, 0b0001);
    properties.flags.isHostInaccessibleAllocation = true;
    auto allocation = memoryManager->allocatePhysicalGraphicsMemory(properties);
    ASSERT_NE(nullptr, allocation);

    auto bankSelector = memoryManager->getLocalMemoryUsageBankSelector(AllocationType::buffer, rootDeviceIndex);
    const auto usedOnBankBeforeMap = bankSelector->getOccupiedMemorySizeForBank(0u);
    EXPECT_NE(0u, usedOnBankBeforeMap);

    MultiGraphicsAllocation mappingA(rootDeviceIndex);
    MultiGraphicsAllocation mappingB(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingA, allocation, 0x100000, properties.size, nullptr, 0u));
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingB, allocation, 0x200000, properties.size, nullptr, 0u));
    auto mappedA = mappingA.getGraphicsAllocation(rootDeviceIndex);
    auto mappedB = mappingB.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedA);
    ASSERT_NE(nullptr, mappedB);

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingA, mappedA, 0x100000, properties.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingB, mappedB, 0x200000, properties.size, osContext, rootDeviceIndex));
    EXPECT_EQ(usedOnBankBeforeMap, bankSelector->getOccupiedMemorySizeForBank(0u));

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMappedPhysicalDeviceMemoryWhenPhysicalAllocationIsFreedFirstThenMemoryBankUsageIsReleasedOnceWithTheLastMapping) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    auto bankSelector = memoryManager->getLocalMemoryUsageBankSelector(AllocationType::buffer, rootDeviceIndex);
    const auto usedOnBankBeforeAllocation = bankSelector->getOccupiedMemorySizeForBank(0u);

    AllocationProperties properties(rootDeviceIndex, true, 8 * MemoryConstants::pageSize64k, AllocationType::buffer, false, false, 0b0001);
    properties.flags.isHostInaccessibleAllocation = true;
    auto allocation = memoryManager->allocatePhysicalGraphicsMemory(properties);
    ASSERT_NE(nullptr, allocation);
    const auto usedOnBankWithAllocation = bankSelector->getOccupiedMemorySizeForBank(0u);
    EXPECT_LT(usedOnBankBeforeAllocation, usedOnBankWithAllocation);

    MultiGraphicsAllocation mappingA(rootDeviceIndex);
    MultiGraphicsAllocation mappingB(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingA, allocation, 0x100000, properties.size, nullptr, 0u));
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingB, allocation, 0x200000, properties.size, nullptr, 0u));
    auto mappedA = mappingA.getGraphicsAllocation(rootDeviceIndex);
    auto mappedB = mappingB.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedA);
    ASSERT_NE(nullptr, mappedB);

    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(usedOnBankWithAllocation, bankSelector->getOccupiedMemorySizeForBank(0u));

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingA, mappedA, 0x100000, properties.size, osContext, rootDeviceIndex));
    EXPECT_EQ(usedOnBankWithAllocation, bankSelector->getOccupiedMemorySizeForBank(0u));

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingB, mappedB, 0x200000, properties.size, osContext, rootDeviceIndex));
    EXPECT_EQ(usedOnBankBeforeAllocation, bankSelector->getOccupiedMemorySizeForBank(0u));

    mock->isVmBindAvailableCall.callParent = true;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMappedPhysicalDeviceMemoryWhenPhysicalAllocationIsFreedFirstThenMappingKeepsPhysicalGmmAndPhysicalIsReleasedWithTheLastMapping) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    AllocationProperties properties(rootDeviceIndex, true, 8 * MemoryConstants::pageSize64k, AllocationType::buffer, false, false, 0b0001);
    properties.flags.isHostInaccessibleAllocation = true;
    auto allocation = memoryManager->allocatePhysicalGraphicsMemory(properties);
    ASSERT_NE(nullptr, allocation);
    auto physicalGmm = allocation->getDefaultGmm();
    ASSERT_NE(nullptr, physicalGmm);
    const auto physicalSize = allocation->getUnderlyingBufferSize();
    const auto usedWithPhysical = memoryManager->getUsedLocalMemorySize(rootDeviceIndex);

    MultiGraphicsAllocation mappingA(rootDeviceIndex);
    MultiGraphicsAllocation mappingB(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingA, allocation, 0x100000, properties.size, nullptr, 0u));
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingB, allocation, 0x200000, properties.size, nullptr, 0u));
    auto mappedA = mappingA.getGraphicsAllocation(rootDeviceIndex);
    auto mappedB = mappingB.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedA);
    ASSERT_NE(nullptr, mappedB);
    EXPECT_EQ(allocation->getNumGmms(), mappedA->getNumGmms());
    EXPECT_EQ(physicalGmm, mappedA->getDefaultGmm());
    EXPECT_EQ(physicalGmm, mappedB->getDefaultGmm());
    EXPECT_EQ(physicalGmm->isCompressionEnabled(), mappedB->isCompressionEnabled());

    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(usedWithPhysical, memoryManager->getUsedLocalMemorySize(rootDeviceIndex));

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingA, mappedA, 0x100000, properties.size, osContext, rootDeviceIndex));
    EXPECT_EQ(usedWithPhysical, memoryManager->getUsedLocalMemorySize(rootDeviceIndex));
    EXPECT_EQ(physicalGmm, mappedB->getDefaultGmm());

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingB, mappedB, 0x200000, properties.size, osContext, rootDeviceIndex));
    EXPECT_EQ(usedWithPhysical - physicalSize, memoryManager->getUsedLocalMemorySize(rootDeviceIndex));

    mock->isVmBindAvailableCall.callParent = true;
}

struct DrmMemoryManagerReleasingPhysicalDuringMappingTeardown : public TestedDrmMemoryManager {
    using TestedDrmMemoryManager::freeGraphicsMemoryImpl;
    DrmMemoryManagerReleasingPhysicalDuringMappingTeardown(ExecutionEnvironment &executionEnvironment)
        : TestedDrmMemoryManager(true, false, false, executionEnvironment) {}

    void unregisterAllocation(GraphicsAllocation *allocation) override {
        if (allocation == mappingToTearDown && !physicalReleaseRequestedDuringTeardown) {
            physicalReleaseRequestedDuringTeardown = true;
            physicalReleaseDeferredDuringTeardown = deferReleaseOfMappedPhysicalMemory(*physicalAllocation);
        }
        TestedDrmMemoryManager::unregisterAllocation(allocation);
    }

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImported) override {
        if (gfxAllocation == physicalAllocation) {
            physicalAllocationReleased = true;
        }
        TestedDrmMemoryManager::freeGraphicsMemoryImpl(gfxAllocation, isImported);
    }

    GraphicsAllocation *mappingToTearDown = nullptr;
    GraphicsAllocation *physicalAllocation = nullptr;
    bool physicalReleaseRequestedDuringTeardown = false;
    bool physicalReleaseDeferredDuringTeardown = false;
    bool physicalAllocationReleased = false;
};

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenLastMappingBeingTornDownWhenPhysicalAllocationIsReleasedConcurrentlyThenReleaseIsDeferredUntilMappingIsFreed) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    DrmMemoryManagerReleasingPhysicalDuringMappingTeardown testedMemoryManager(*executionEnvironment);
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize64k, AllocationType::buffer, false, false, 0b0001);
    properties.flags.isHostInaccessibleAllocation = true;
    auto allocation = testedMemoryManager.allocatePhysicalGraphicsMemory(properties);
    ASSERT_NE(nullptr, allocation);

    MultiGraphicsAllocation mapping(rootDeviceIndex);
    EXPECT_TRUE(testedMemoryManager.mapPhysicalDeviceMemoryToVirtualMemory(mapping, allocation, 0x100000, properties.size, nullptr, 0u));
    auto mappedAllocation = mapping.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedAllocation);
    testedMemoryManager.mappingToTearDown = mappedAllocation;
    testedMemoryManager.physicalAllocation = allocation;

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(testedMemoryManager.unMapPhysicalDeviceMemoryFromVirtualMemory(mapping, mappedAllocation, 0x100000, properties.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(testedMemoryManager.physicalReleaseDeferredDuringTeardown);
    EXPECT_TRUE(testedMemoryManager.physicalAllocationReleased);

    if (!testedMemoryManager.physicalAllocationReleased) {
        testedMemoryManager.freeGraphicsMemory(allocation);
    }
    mock->isVmBindAvailableCall.callParent = true;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNonVmBindSystemWhenPhysicalDeviceMemoryMappedToMultipleVirtualAddressesThenMappingsShareThePhysicalHandle) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);

    auto physicalHandle = allocation->getBOs()[0]->peekHandle();

    MultiGraphicsAllocation mappingA(rootDeviceIndex);
    MultiGraphicsAllocation mappingB(rootDeviceIndex);

    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingA, allocation, 0x100000, allocData.size, nullptr, 0u));
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(mappingB, allocation, 0x200000, allocData.size, nullptr, 0u));

    auto mappedA = static_cast<DrmAllocation *>(mappingA.getGraphicsAllocation(rootDeviceIndex));
    auto mappedB = static_cast<DrmAllocation *>(mappingB.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedA);
    ASSERT_NE(nullptr, mappedB);
    EXPECT_NE(mappedA, mappedB);
    EXPECT_EQ(physicalHandle, mappedA->getBOs()[0]->peekHandle());
    EXPECT_EQ(physicalHandle, mappedB->getBOs()[0]->peekHandle());

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingA, mappedA, 0x100000, allocData.size, osContext, rootDeviceIndex));
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(mappingB, mappedB, 0x200000, allocData.size, osContext, rootDeviceIndex));

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMappedPhysicalDeviceMemoryWhenUnMappedThenPhysicalAllocationIsLeftUntouched) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    uint64_t gpuAddress = 0x1234;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &physicalBos = drmAllocation->getBOs();
    std::array<uint64_t, EngineLimits::maxHandleCount> physicalBoAddresses{};
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        ASSERT_NE(nullptr, physicalBos[handleId]);
        physicalBoAddresses[handleId] = physicalBos[handleId]->peekAddress();
    }

    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, 0u));

    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    EXPECT_NE(mappedAllocation, allocation);

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, rootDeviceIndex));

    EXPECT_EQ(0u, allocation->getGpuAddress());
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = physicalBos[handleId];
        ASSERT_NE(nullptr, bo);
        EXPECT_EQ(physicalBoAddresses[handleId], bo->peekAddress());
        EXPECT_EQ(0u, bo->getPhysicalMemoryOffset());
        EXPECT_EQ(0u, bo->getVirtualMappingSize());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenVmBindAvailableWhenMappingPhysicalDeviceMemoryWithOffsetThenBufferObjectIsPlacedAtReservedRangeAndOffsetIsCarriedByBind) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 8 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    uint64_t gpuAddress = 0x100000;
    size_t offset = 2 * MemoryConstants::pageSize;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    auto physicalBo = allocation->getBOs()[0];
    ASSERT_NE(nullptr, physicalBo);
    auto baseAddress = physicalBo->peekAddress();
    auto gmmHelper = device->getGmmHelper();

    MultiGraphicsAllocation multiGraphicsAllocation(allocation->getRootDeviceIndex());
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, offset));

    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(allocation->getRootDeviceIndex()));
    ASSERT_NE(nullptr, mappedAllocation);
    auto bo = mappedAllocation->getBOs()[0];
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(gmmHelper->canonize(gpuAddress + baseAddress), bo->peekAddress());
    EXPECT_EQ(offset, bo->getPhysicalMemoryOffset());
    EXPECT_EQ(allocData.size, bo->getVirtualMappingSize());
    EXPECT_EQ(baseAddress, physicalBo->peekAddress());
    EXPECT_EQ(0u, physicalBo->getPhysicalMemoryOffset());
    EXPECT_EQ(0u, physicalBo->getVirtualMappingSize());

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, allocation->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(allocation->getRootDeviceIndex()));

    EXPECT_EQ(baseAddress, physicalBo->peekAddress());
    EXPECT_EQ(0u, physicalBo->getPhysicalMemoryOffset());
    EXPECT_EQ(0u, physicalBo->getVirtualMappingSize());

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenVmBindUnavailableWhenMappingPhysicalDeviceMemoryWithOffsetThenOffsetIsFoldedIntoBufferObjectAddressAndRestoredOnUnmap) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 8 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    uint64_t gpuAddress = 0x100000;
    size_t offset = 2 * MemoryConstants::pageSize;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    auto physicalBo = allocation->getBOs()[0];
    ASSERT_NE(nullptr, physicalBo);
    auto baseAddress = physicalBo->peekAddress();
    auto gmmHelper = device->getGmmHelper();

    MultiGraphicsAllocation multiGraphicsAllocation(allocation->getRootDeviceIndex());
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, offset));

    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(allocation->getRootDeviceIndex()));
    ASSERT_NE(nullptr, mappedAllocation);
    auto bo = mappedAllocation->getBOs()[0];
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(gmmHelper->canonize(gpuAddress + baseAddress - offset), bo->peekAddress());
    EXPECT_EQ(offset, bo->getPhysicalMemoryOffset());
    EXPECT_EQ(allocData.size, bo->getVirtualMappingSize());
    EXPECT_EQ(baseAddress, physicalBo->peekAddress());
    EXPECT_EQ(0u, physicalBo->getPhysicalMemoryOffset());
    EXPECT_EQ(0u, physicalBo->getVirtualMappingSize());

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, allocation->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(allocation->getRootDeviceIndex()));

    EXPECT_EQ(baseAddress, physicalBo->peekAddress());
    EXPECT_EQ(0u, physicalBo->getPhysicalMemoryOffset());
    EXPECT_EQ(0u, physicalBo->getVirtualMappingSize());

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMultiTilePhysicalDeviceMemoryWhenMappingWithNonZeroOffsetThenMappingIsRejected) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    uint64_t gpuAddress = 0x100000;
    size_t offset = 2 * MemoryConstants::pageSize;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

    auto &bos = allocation->getBOs();
    std::array<uint64_t, EngineLimits::maxHandleCount> baseAddresses{};
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        ASSERT_NE(nullptr, bos[handleId]);
        baseAddresses[handleId] = bos[handleId]->peekAddress();
    }

    // The chunked multi-BO path has no per-bind offset, so a non-zero offset cannot be
    // honored - the map must fail rather than silently bind each chunk at offset 0.
    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_FALSE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, offset));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        EXPECT_EQ(baseAddresses[handleId], bo->peekAddress());
        EXPECT_EQ(0u, bo->getPhysicalMemoryOffset());
        EXPECT_EQ(0u, bo->getVirtualMappingSize());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenVmBindAvailableWhenMappingMultiTilePhysicalDeviceMemoryAtOffsetZeroThenEachChunkKeepsItsOwnSizeForBinding) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    uint64_t gpuAddress = 0x100000;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalLocalDeviceMemory(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

    auto &bos = allocation->getBOs();
    auto gmmHelper = device->getGmmHelper();
    std::array<uint64_t, EngineLimits::maxHandleCount> baseAddresses{};
    std::array<size_t, EngineLimits::maxHandleCount> chunkSizes{};
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        ASSERT_NE(nullptr, bos[handleId]);
        baseAddresses[handleId] = bos[handleId]->peekAddress();
        chunkSizes[handleId] = bos[handleId]->peekSize();
    }

    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, 0u));

    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    auto &mappedBos = mappedAllocation->getBOs();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = mappedBos[handleId];
        ASSERT_NE(nullptr, bo);
        EXPECT_EQ(gmmHelper->canonize(gpuAddress + baseAddresses[handleId]), bo->peekAddress());
        EXPECT_EQ(0u, bo->getPhysicalMemoryOffset());
        // virtualMappingSize must stay 0 so changeBufferObjectBinding binds each chunk at
        // its own peekSize() rather than the full allocation size (which would overlap).
        EXPECT_EQ(0u, bo->getVirtualMappingSize());
        EXPECT_EQ(chunkSizes[handleId], bo->peekSize());
    }

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, allocation->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        EXPECT_EQ(baseAddresses[handleId], bo->peekAddress());
        EXPECT_EQ(0u, bo->getPhysicalMemoryOffset());
        EXPECT_EQ(0u, bo->getVirtualMappingSize());
    }

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryWhenRegisterSysMemAllocFailsThenMappingReturnsFalse) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());

    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);

    uint64_t gpuAddress = 0x100000;
    memoryManager->failRegisterSysMemAlloc = true;
    EXPECT_FALSE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, 0u));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    EXPECT_EQ(0u, allocation->getGpuAddress());

    memoryManager->failRegisterSysMemAlloc = false;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUnmappedPhysicalDeviceMemoryMappingWhenRegisterSysMemAllocFailsOnMappingSameRangeAgainThenMappingStaysParkedAndMappingReturnsFalse) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());

    const uint64_t gpuRange = 0x100000;
    MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    ASSERT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuRange, allocData.size, nullptr, 0u));
    auto mappedAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedAllocation);
    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuRange, allocData.size, osContext, rootDeviceIndex));

    memoryManager->failRegisterSysMemAlloc = true;
    EXPECT_FALSE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuRange, allocData.size, nullptr, 0u));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(0u, allocation->getPhysicalMappingsInfo().liveMappingsCount);
    ASSERT_EQ(1u, allocation->getPhysicalMappingsInfo().parkedMappings.size());
    EXPECT_EQ(mappedAllocation, allocation->getPhysicalMappingsInfo().parkedMappings[0].mappedAllocation);

    memoryManager->failRegisterSysMemAlloc = false;
    EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuRange, allocData.size, nullptr, 0u));
    EXPECT_EQ(mappedAllocation, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuRange, allocData.size, osContext, rootDeviceIndex));
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPhysicalDeviceMemoryWhenMappedAndUnmappedThenUsedSystemMemorySizeIsNotAccounted) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = 0b0001;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocatePhysicalDeviceMemory(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());

    const auto usedSystemMemorySizeBefore = memoryManager->getUsedSystemMemorySize();
    uint64_t gpuAddress = 0x100000;
    auto osContext = device->getDefaultEngine().osContext;

    for (uint32_t i = 0; i < 4u; i++) {
        MultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
        EXPECT_TRUE(memoryManager->mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, allocation, gpuAddress, allocData.size, nullptr, 0u));
        auto mappedAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
        ASSERT_NE(nullptr, mappedAllocation);
        EXPECT_EQ(usedSystemMemorySizeBefore, memoryManager->getUsedSystemMemorySize());

        EXPECT_TRUE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, mappedAllocation, gpuAddress, allocData.size, osContext, rootDeviceIndex));
        EXPECT_EQ(usedSystemMemorySizeBefore, memoryManager->getUsedSystemMemorySize());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithLargeBufferWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithContiguousGpuVirtualAddressesAndPartialSizes) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.colouringPolicy = ColouringPolicy::chunkSizeBased;
    allocData.storageInfo.colouringGranularity = 256 * MemoryConstants::kiloByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(allocation->storageInfo.colouringPolicy, ColouringPolicy::chunkSizeBased);
    EXPECT_EQ(allocation->storageInfo.colouringGranularity, 256 * MemoryConstants::kiloByte);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto numHandles = allocation->getNumGmms();
    EXPECT_EQ(numHandles, drmAllocation->getBOs().size());

    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        EXPECT_EQ(boAddress, bo->peekAddress());
        boAddress += bo->peekSize();
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMultipleHandlesUpdateSubAllocationInformation) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.colouringPolicy = ColouringPolicy::chunkSizeBased;
    allocData.storageInfo.colouringGranularity = 256 * MemoryConstants::kiloByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(allocation->storageInfo.colouringPolicy, ColouringPolicy::chunkSizeBased);
    EXPECT_EQ(allocation->storageInfo.colouringGranularity, 256 * MemoryConstants::kiloByte);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto numHandles = allocation->getNumGmms();
    EXPECT_EQ(numHandles, drmAllocation->getBOs().size());

    auto &bos = drmAllocation->getBOs();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        EXPECT_EQ(allocation->getHandleAddressBase(handleId), bo->peekAddress());
        EXPECT_EQ(allocation->getHandleSize(handleId), bo->peekSize());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocationInDevicePoolAndDeviceBitfieldWithHolesThenCorrectAllocationCreated) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = 0b1011;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto kernelIsaAllocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));

    EXPECT_NE(nullptr, kernelIsaAllocation);

    auto gpuAddress = kernelIsaAllocation->getGpuAddress();
    auto &bos = kernelIsaAllocation->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_EQ(gpuAddress, bos[1]->peekAddress());

    EXPECT_EQ(nullptr, bos[2]);

    EXPECT_NE(nullptr, bos[3]);
    EXPECT_EQ(gpuAddress, bos[3]->peekAddress());

    auto &storageInfo = kernelIsaAllocation->storageInfo;
    EXPECT_EQ(0b1011u, storageInfo.memoryBanks.to_ulong());

    memoryManager->freeGraphicsMemory(kernelIsaAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesWhenMappingPhysicalHostMemoryThenMappingIsSuccessful) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));
    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, true));
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

namespace {
DrmMockCustom *getRootDeviceDrmMock(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    return static_cast<DrmMockCustom *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
}
} // namespace

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMultipleRootDevicesWhenMappingPhysicalHostMemoryThenEachMappingAllocationHasOwnBufferGmmAndBoPatIsQueriedWithIt) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    const uint64_t mappedGpuAddress = 0x1234;

    auto freeAllocation = [this](GraphicsAllocation *allocation) { this->memoryManager->freeGraphicsMemory(allocation); };
    std::unique_ptr<GraphicsAllocation, decltype(freeAllocation)> physicalAllocation(memoryManager->allocatePhysicalHostMemory(allocData, status), freeAllocation);
    ASSERT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    for (const auto index : rootDeviceIndices) {
        getRootDeviceDrmMock(*executionEnvironment, index)->vmBindPatIndexProgrammingSupported = true;
    }
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    ASSERT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation.get(), mappedGpuAddress, allocData.size, 0u, true));
    auto unmap = [&](MultiGraphicsAllocation *mapping) {
        EXPECT_TRUE(this->memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(*mapping, physicalAllocation.get(), mappedGpuAddress, allocData.size, true));
    };
    std::unique_ptr<MultiGraphicsAllocation, decltype(unmap)> mapping(&multiGraphicsAllocation, unmap);

    for (const auto index : rootDeviceIndices) {
        auto mappingAllocation = multiGraphicsAllocation.getGraphicsAllocation(index);
        ASSERT_NE(nullptr, mappingAllocation);
        auto gmm = mappingAllocation->getDefaultGmm();
        ASSERT_NE(nullptr, gmm);
        EXPECT_NE(physicalAllocation->getDefaultGmm(), gmm);
        EXPECT_EQ(memoryManager->getGmmHelper(index), gmm->getGmmHelper());
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[index];
        const auto expectedUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::bufferHostMemory, false, rootDeviceEnvironment.getProductHelper(), *rootDeviceEnvironment.getHardwareInfo());
        EXPECT_EQ(expectedUsage, gmm->getResourceUsageType());
        auto mockClientContext = static_cast<MockGmmClientContextBase *>(rootDeviceEnvironment.getGmmClientContext());
        EXPECT_EQ(gmm->gmmResourceInfo->peekGmmResourceInfo(), mockClientContext->passedResourceInfoForGetPatIndexQuery);
    }
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMultipleRootDevicesWhenMappingPhysicalHostMemoryThenSharedHandleIsImportedThroughEachImportingDeviceDrm) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    auto ownerDrm = getRootDeviceDrmMock(*executionEnvironment, rootDeviceIndex);
    auto importDrm0 = getRootDeviceDrmMock(*executionEnvironment, 0u);
    auto importDrm2 = getRootDeviceDrmMock(*executionEnvironment, 2u);
    const auto ownerImportsBefore = ownerDrm->ioctlCnt.primeFdToHandle.load();
    const auto importDrm0ImportsBefore = importDrm0->ioctlCnt.primeFdToHandle.load();
    const auto importDrm2ImportsBefore = importDrm2->ioctlCnt.primeFdToHandle.load();

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    ASSERT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));

    EXPECT_EQ(ownerImportsBefore, ownerDrm->ioctlCnt.primeFdToHandle.load());
    EXPECT_EQ(importDrm0ImportsBefore + 1, importDrm0->ioctlCnt.primeFdToHandle.load());
    EXPECT_EQ(importDrm2ImportsBefore + 1, importDrm2->ioctlCnt.primeFdToHandle.load());
    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        auto allocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(i));
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(getRootDeviceDrmMock(*executionEnvironment, i), allocation->getBO()->peekDrm());
    }

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, true));
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMultipleRootDevicesWhenMappingPhysicalHostMemoryWithOffsetThenEachDeviceBufferObjectCarriesTheOffsetAndMappingSize) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    enableMmapWindowRelocation(mock);
    resetCapturedMremap();
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 4 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;
    size_t offset = 2 * MemoryConstants::pageSize;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    auto gmmHelper = device->getGmmHelper();
    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(i));
        ASSERT_NE(nullptr, mappedAllocation);
        auto mappedBo = mappedAllocation->getBO();
        ASSERT_NE(nullptr, mappedBo);
        EXPECT_EQ(gmmHelper->canonize(gpuAddress), mappedBo->peekAddress());
        EXPECT_EQ(offset, mappedBo->getPhysicalMemoryOffset());
        EXPECT_EQ(mappingSize, mappedBo->getVirtualMappingSize());
    }

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, true));
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenVmBindAvailableAndZeroOffsetWhenMmapFailsThenMappingPhysicalHostMemoryReturnsFalse) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    resetCapturedMremap();
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x100000;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = [](void *, size_t, int, int, int, off_t) throw()->void * { return MAP_FAILED; };

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, 0u, true));

    EXPECT_EQ(0u, capturedMremapCalls);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMmapFailsForOffsetWindowWhenMappingPhysicalHostMemoryThenFalseIsReturnedGracefully) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x100000;
    size_t offset = MemoryConstants::pageSize;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = [](void *, size_t, int, int, int, off_t) throw()->void * { return MAP_FAILED; };

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenVmBindUnavailableWhenMappingPhysicalHostMemoryWithOffsetThenMmapTokenIsUnmodifiedAndBaseFoldsTheOffset) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    capturedHostMmapAddr = nullptr;
    capturedHostMmapLen = 0u;
    capturedHostMmapOffset = 0;
    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = capturingHostMmap;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x100000;
    size_t offset = MemoryConstants::pageSize;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);
    auto token = physicalAllocation->getBO()->getMmapOffset();

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    EXPECT_EQ(static_cast<off_t>(token), capturedHostMmapOffset);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress - offset), capturedHostMmapAddr);
    EXPECT_EQ(offset + mappingSize, capturedHostMmapLen);

    auto mappedBo = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex))->getBO();
    ASSERT_NE(nullptr, mappedBo);
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(gpuAddress - offset), mappedBo->peekAddress());
    EXPECT_EQ(offset + mappingSize, mappedBo->peekSize());

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, true));

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenPrimeFdToHandleFailingWhenMappingPhysicalHostMemoryThenReservationPlaceholderIsRestoredOnRollback) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = capturingHostMmap;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x100000;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};

    capturedHostMmapAddr = nullptr;
    capturedHostMmapLen = 0u;
    capturedHostMmapProt = -1;
    capturedHostMmapFlags = 0;
    auto importDrm = getRootDeviceDrmMock(*executionEnvironment, 0u);
    importDrm->failOnPrimeFdToHandle = true;

    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, 0u, true));

    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress), capturedHostMmapAddr);
    EXPECT_EQ(mappingSize, capturedHostMmapLen);
    EXPECT_EQ(PROT_NONE, capturedHostMmapProt);
    EXPECT_NE(0, capturedHostMmapFlags & MAP_ANONYMOUS);
    EXPECT_NE(0, capturedHostMmapFlags & MAP_FIXED);

    importDrm->failOnPrimeFdToHandle = false;
    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenNonSvmReservationAndPrimeFdToHandleFailingWhenMappingPhysicalHostMemoryThenRangeIsReleasedInsteadOfLeavingAPlaceholder) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = capturingHostMmap;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x100000;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};

    capturedHostMmapProt = -1;
    auto importDrm = getRootDeviceDrmMock(*executionEnvironment, 0u);
    importDrm->failOnPrimeFdToHandle = true;
    auto munmapCallsBefore = SysCalls::munmapFuncCalled;

    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, 0u, false));

    EXPECT_EQ(munmapCallsBefore + 1u, SysCalls::munmapFuncCalled);
    EXPECT_EQ(PROT_READ | PROT_WRITE, capturedHostMmapProt);

    importDrm->failOnPrimeFdToHandle = false;
    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

struct PhysicalHostOffsetMapFixture : DrmMemoryManagerWithExplicitExpectationsTest {
    DrmAllocation *createPhysicalHostAllocation() {
        allocData.allFlags = 0;
        allocData.size = 4 * MemoryConstants::pageSize;
        allocData.flags.allocateMemory = true;
        allocData.flags.isUSMHostAllocation = true;
        allocData.type = AllocationType::bufferHostMemory;
        allocData.storageInfo.multiStorage = false;
        allocData.rootDeviceIndex = rootDeviceIndex;
        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
        return static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    }
    AllocationData allocData{};
    static constexpr uint64_t gpuAddress = 0x100000;
    static constexpr size_t offset = MemoryConstants::pageSize;
    static constexpr size_t mappingSize = MemoryConstants::pageSize;
};

TEST_F(PhysicalHostOffsetMapFixture, givenNonSvmReservationWhenUnmappingPhysicalHostMemoryThenRangeIsReleasedInsteadOfLeavingAPlaceholder) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;
    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = capturingHostMmap;

    auto physicalAllocation = createPhysicalHostAllocation();
    ASSERT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    ASSERT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, 0u, true));

    capturedHostMmapAddr = nullptr;
    capturedHostMmapProt = -1;
    auto munmapCallsBefore = SysCalls::munmapFuncCalled;

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, false));

    EXPECT_EQ(munmapCallsBefore + 1u, SysCalls::munmapFuncCalled);
    EXPECT_EQ(nullptr, capturedHostMmapAddr);

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(PhysicalHostOffsetMapFixture, givenWholeObjectMmapFailingWhenRelocatingWindowThenMappingIsRejectedBeforeRemap) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    enableMmapWindowRelocation(mock);
    resetCapturedMremap();
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    auto physicalAllocation = createPhysicalHostAllocation();
    ASSERT_NE(nullptr, physicalAllocation);

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = [](void *, size_t, int, int, int, off_t) throw()->void * { return MAP_FAILED; };

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    EXPECT_EQ(0u, capturedMremapCalls);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(PhysicalHostOffsetMapFixture, givenWindowRelocationSupportThenPhysicalHostMemoryOffsetFoldIsRequiredOnlyWithoutIt) {
    EXPECT_TRUE(memoryManager->isPhysicalHostMemoryOffsetFoldRequired(rootDeviceIndex));
    enableMmapWindowRelocation(mock);
    EXPECT_FALSE(memoryManager->isPhysicalHostMemoryOffsetFoldRequired(rootDeviceIndex));
}

#if defined(__linux__)
TEST_F(PhysicalHostOffsetMapFixture, givenRangeOverlappingAnExistingMappingWhenReservingExactCpuAddressThenClaimIsRejected) {
    memoryManager->mmapFixedNoReplaceFunction = [](void *address, size_t size) noexcept -> void * {
        return ::mmap(address, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | MAP_NORESERVE, -1, 0);
    };

    const size_t claimSize = 2 * MemoryConstants::pageSize;
    auto occupied = ::mmap(nullptr, claimSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    ASSERT_NE(MAP_FAILED, occupied);

    EXPECT_FALSE(memoryManager->reserveExactCpuAddress(castToUint64(occupied), claimSize));
    EXPECT_FALSE(memoryManager->reserveExactCpuAddress(castToUint64(occupied) + MemoryConstants::pageSize, MemoryConstants::pageSize));

    ::munmap(occupied, claimSize);
    EXPECT_TRUE(memoryManager->reserveExactCpuAddress(castToUint64(occupied), claimSize));
    ::munmap(occupied, claimSize);

    memoryManager->mmapFixedNoReplaceFunction = SysCalls::mmapFixedNoReplace;
}
#endif

TEST_F(PhysicalHostOffsetMapFixture, givenReserveExactCpuAddressThenClaimIsReportedFromTheUnderlyingMapping) {
    resetCapturedNoReplace();
    auto originalNoReplace = memoryManager->mmapFixedNoReplaceFunction;
    memoryManager->mmapFixedNoReplaceFunction = capturingMmapFixedNoReplace;

    EXPECT_TRUE(memoryManager->reserveExactCpuAddress(gpuAddress - offset, offset));
    EXPECT_EQ(1u, capturedNoReplaceCalls);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress - offset), capturedNoReplaceAddress);
    EXPECT_EQ(offset, capturedNoReplaceSize);

    failCapturedNoReplace = true;
    EXPECT_FALSE(memoryManager->reserveExactCpuAddress(gpuAddress - offset, offset));
    EXPECT_EQ(2u, capturedNoReplaceCalls);

    failCapturedNoReplace = false;
    memoryManager->mmapFixedNoReplaceFunction = originalNoReplace;
}

TEST_F(PhysicalHostOffsetMapFixture, givenVmBindAvailableButWindowRelocationUnsupportedWhenMappingWithOffsetThenOffsetIsFoldedInsteadOfMappingObjectStart) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    resetCapturedNoReplace();
    resetCapturedMremap();
    auto originalNoReplace = memoryManager->mmapFixedNoReplaceFunction;
    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFixedNoReplaceFunction = capturingMmapFixedNoReplace;
    memoryManager->mremapFixedFunction = capturingMremapFixed;
    memoryManager->mmapFunction = capturingHostMmap;

    auto physicalAllocation = createPhysicalHostAllocation();
    ASSERT_NE(nullptr, physicalAllocation);
    auto token = physicalAllocation->getBO()->getMmapOffset();

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    EXPECT_EQ(0u, capturedMremapCalls);
    EXPECT_EQ(static_cast<off_t>(token), capturedHostMmapOffset);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress - offset), capturedHostMmapAddr);
    EXPECT_EQ(offset + mappingSize, capturedHostMmapLen);

    auto mappedBo = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex))->getBO();
    ASSERT_NE(nullptr, mappedBo);
    EXPECT_EQ(device->getGmmHelper()->canonize(gpuAddress), mappedBo->peekAddress());
    EXPECT_EQ(offset, mappedBo->getPhysicalMemoryOffset());
    EXPECT_EQ(mappingSize, mappedBo->getVirtualMappingSize());

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, true));
    memoryManager->mmapFunction = originalMmap;
    memoryManager->mmapFixedNoReplaceFunction = originalNoReplace;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenPhysicalHostMemoryMappedWithOffsetWhenUnmappingThenReservationPlaceholderIsRestoredInsteadOfReleasingTheRange) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = capturingHostMmap;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x100000;
    size_t offset = MemoryConstants::pageSize;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices};
    ASSERT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    capturedHostMmapAddr = nullptr;
    capturedHostMmapLen = 0u;
    capturedHostMmapProt = -1;
    capturedHostMmapFlags = 0;
    auto munmapCallsBeforeUnmap = SysCalls::munmapFuncCalled;

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, true));

    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress - offset), capturedHostMmapAddr);
    EXPECT_EQ(offset + mappingSize, capturedHostMmapLen);
    EXPECT_EQ(PROT_NONE, capturedHostMmapProt);
    EXPECT_NE(0, capturedHostMmapFlags & MAP_ANONYMOUS);
    EXPECT_NE(0, capturedHostMmapFlags & MAP_FIXED);
    EXPECT_EQ(munmapCallsBeforeUnmap, SysCalls::munmapFuncCalled);

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenVmBindUnavailableWhenMappingSinglePhysicalHostMemoryToMultipleVirtualRangesThenMappingsAreNotForcedIntoGlobalResidency) {
    // On the non-vmBind (softpin/execbuffer) path every VA mapping of one physical allocation
    // shares the same GEM handle. Forcing them into the persistent global residency would place
    // the same handle at conflicting addresses in a single execbuffer, which the kernel rejects.
    // The map path must therefore leave residency to the referencing command list.
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    // The fixture's device already registers engines with the memory manager, so the pre-fix map
    // path would have iterated them and pushed each mapping into the operations handler's residency
    // list. The assertions below therefore fail on the old (forced-residency) behaviour and pass on
    // the fixed one, making this a genuine regression guard.
    ASSERT_FALSE(memoryManager->getRegisteredEngines(rootDeviceIndex).empty());

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 2 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);

    uint64_t gpuAddress0 = 0x100000;
    uint64_t gpuAddress1 = 0x200000;
    MultiGraphicsAllocation multiGraphicsAllocation0{numRootDevices};
    MultiGraphicsAllocation multiGraphicsAllocation1{numRootDevices};

    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation0, physicalAllocation, gpuAddress0, allocData.size, 0u, true));
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation1, physicalAllocation, gpuAddress1, allocData.size, 0u, true));

    auto mappedAllocation0 = multiGraphicsAllocation0.getGraphicsAllocation(rootDeviceIndex);
    auto mappedAllocation1 = multiGraphicsAllocation1.getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, mappedAllocation0);
    ASSERT_NE(nullptr, mappedAllocation1);

    auto drmMemoryOperationsHandler = static_cast<DrmMemoryOperationsHandler *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, drmMemoryOperationsHandler->isResident(device.get(), *mappedAllocation0));
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, drmMemoryOperationsHandler->isResident(device.get(), *mappedAllocation1));

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation0, physicalAllocation, gpuAddress0, allocData.size, true));
    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation1, physicalAllocation, gpuAddress1, allocData.size, true));

    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesAndPrimeHandleToFdFailsWhenMappingPhysicalHostMemoryThenMappingFails) {
    mock->failOnPrimeHandleToFd = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));

    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesAndPrimeFdToHandleFailsWhenMappingPhysicalHostMemoryThenMappingFails) {
    getRootDeviceDrmMock(*executionEnvironment, 0u)->failOnPrimeFdToHandle = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));

    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDeviceBitfieldWithHolesAndSecondPrimeFdToHandleFailsWhenMappingPhysicalHostMemoryThenMappingFails) {
    getRootDeviceDrmMock(*executionEnvironment, 2u)->failOnPrimeFdToHandle = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    rootDeviceIndices.pushUnique(2);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));

    for (uint32_t i = 0; i < static_cast<uint32_t>(rootDeviceIndices.size()); i++) {
        EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(i));
    }

    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceWhenMappingPhysicalHostMemoryThenMappingIsSuccessful) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));
    EXPECT_NE(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, true));
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceWhenMappingPhysicalHostMemoryWithOffsetThenWindowIsRelocatedOntoTheReservationAndCarriedByGpuBind) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    enableMmapWindowRelocation(mock);
    resetCapturedMremap();
    resetCapturedNoReplace();
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 4 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;
    size_t offset = 2 * MemoryConstants::pageSize;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);
    auto physicalMmapOffset = physicalAllocation->getBO()->getMmapOffset();

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    auto mappedAllocation = static_cast<DrmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    ASSERT_NE(nullptr, mappedAllocation);
    auto mappedBo = mappedAllocation->getBO();
    ASSERT_NE(nullptr, mappedBo);

    EXPECT_EQ(physicalMmapOffset, mappedBo->getMmapOffset());
    EXPECT_EQ(device->getGmmHelper()->canonize(gpuAddress), mappedBo->peekAddress());
    EXPECT_EQ(offset, mappedBo->getPhysicalMemoryOffset());
    EXPECT_EQ(mappingSize, mappedBo->getVirtualMappingSize());

    EXPECT_EQ(1u, capturedMremapCalls);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress), capturedMremapNewAddress);
    EXPECT_EQ(mappingSize, capturedMremapSize);
    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress), mappedAllocation->getMmapPtr());
    EXPECT_EQ(mappingSize, mappedAllocation->getMmapSize());
    EXPECT_EQ(0u, capturedNoReplaceCalls);

    capturedHostMmapAddr = nullptr;
    capturedHostMmapLen = 0u;
    capturedHostMmapProt = -1;
    capturedHostMmapFlags = 0;
    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = capturingHostMmap;

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, true));

    EXPECT_EQ(reinterpret_cast<void *>(gpuAddress), capturedHostMmapAddr);
    EXPECT_EQ(mappingSize, capturedHostMmapLen);
    EXPECT_EQ(PROT_NONE, capturedHostMmapProt);
    EXPECT_NE(0, capturedHostMmapFlags & MAP_ANONYMOUS);
    EXPECT_NE(0, capturedHostMmapFlags & MAP_FIXED);

    memoryManager->mmapFunction = originalMmap;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMremapFailingWhenMappingPhysicalHostMemoryWithOffsetThenWholeObjectMappingIsReleasedAndFalseIsReturned) {
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = true;
    enableMmapWindowRelocation(mock);
    resetCapturedMremap();
    failCapturedMremap = true;
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 4 * MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;
    size_t offset = 2 * MemoryConstants::pageSize;
    size_t mappingSize = MemoryConstants::pageSize;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    ASSERT_NE(nullptr, physicalAllocation);

    auto munmapCallsBefore = SysCalls::munmapFuncCalled;

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, mappingSize, offset, true));

    EXPECT_EQ(1u, capturedMremapCalls);
    EXPECT_EQ(munmapCallsBefore + 1u, SysCalls::munmapFuncCalled);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    failCapturedMremap = false;
    mock->isVmBindAvailableCall.callParent = true;
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceWhenMappingLockedPhysicalHostMemoryThenMappingIsSuccessful) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);
    physicalAllocation->lock(addrToPtr(gpuAddress));

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));
    EXPECT_NE(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
    multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)->lock(addrToPtr(gpuAddress));

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, true));
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSingleRootDeviceAndPrimeHandleToFdFailsWhenMappingPhysicalHostMemoryThenMappingIsSuccessful) {
    mock->failOnPrimeHandleToFd = true;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.isUSMHostAllocation = true;
    allocData.type = AllocationType::bufferHostMemory;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;
    uint64_t gpuAddress = 0x1234;

    auto physicalAllocation = static_cast<DrmAllocation *>(memoryManager->allocatePhysicalHostMemory(allocData, status));
    EXPECT_NE(nullptr, physicalAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);
    MultiGraphicsAllocation multiGraphicsAllocation{1u};
    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, 0u, true));
    EXPECT_NE(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
    EXPECT_NE(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuAddress, allocData.size, true));
    memoryManager->freeGraphicsMemory(physicalAllocation);
}

struct DrmMemoryManagerLocalMemoryAlignmentTest : DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest {
    std::unique_ptr<TestedDrmMemoryManager> createMemoryManager() {
        return std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);
    }

    bool isAllocationWithinHeap(MemoryManager &memoryManager, const GraphicsAllocation &allocation, HeapIndex heap) {
        const auto allocationStart = allocation.getGpuAddress();
        const auto allocationEnd = allocationStart + allocation.getUnderlyingBufferSize();
        const auto gmmHelper = device->getGmmHelper();
        const auto heapStart = gmmHelper->canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapBase(heap));
        const auto heapEnd = gmmHelper->canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapLimit(heap));
        return heapStart <= allocationStart && allocationEnd <= heapEnd;
    }

    const uint32_t rootDeviceIndex = 1u;
    DebugManagerStateRestore restore{};
};

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenLocalMemoryAlignmentWhenAllocatingSmallAllocationThenUsePlatformAppropriateHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
        auto expectedHeap = productHelper.is2MBLocalMemAlignmentEnabled() ? HeapIndex::heapStandard2MB : HeapIndex::heapStandard64KB;
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, expectedHeap));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
        auto expectedHeap = productHelper.is2MBLocalMemAlignmentEnabled() ? HeapIndex::heapStandard2MB : HeapIndex::heapStandard64KB;
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, expectedHeap));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenLocalMemoryAlignmentWhenAllocatingLargeAllocationThenUse2MbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenExtendedHeapPreferredAnd2MbAlignmentAllowedWhenAllocatingAllocationBiggerThenUseExtendedHeap) {
    if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended) == 0) {
        GTEST_SKIP();
    }

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = false;
    MemoryManager::AllocationStatus allocationStatus;

    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapExtended));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapExtended));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapExtended));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationBiggerThanTheAlignmentThenAlignProperly) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // size==2MB, use 2MB heap
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::megaByte);
        allocationData.size = 2 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 2 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size > 2MB, use 2MB heap
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(16 * MemoryConstants::megaByte);
        allocationData.size = 16 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 16 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(8 * MemoryConstants::pageSize64k);
        allocationData.size = 8 * MemoryConstants::pageSize64k;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
        auto expectedHeap = productHelper.is2MBLocalMemAlignmentEnabled() ? HeapIndex::heapStandard2MB : HeapIndex::heapStandard64KB;
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, expectedHeap));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 8 * MemoryConstants::pageSize64k));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationLessThanTheAlignmentThenIgnoreCustomAlignment) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 3 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // Too small allocation, fallback to 2MB heap
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // Too small allocation, fallback to 2MB heap
        debugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenForced2MBSizeAlignmentWhenAllocatingAllocationThenUseProperAlignment) {
    debugManager.flags.ExperimentalAlignLocalMemorySizeTo2MB.set(true);

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        allocationData.size = 1;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_EQ(2 * MemoryConstants::megaByte, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(2 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        allocationData.size = 2 * MemoryConstants::megaByte + 1;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenEnabled2MBSizeAlignmentWhenAllocatingAllocationThenUseProperAlignment) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    allocationData.size = 2 * MemoryConstants::megaByte + 1;
    auto memoryManager = createMemoryManager();
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
    EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
    EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(4 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenEnabled2MBSizeAlignmentWhenAllocatingLargeKernelIsaThenAllocationIsAlignedTo2MB) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize2M + 1;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto memoryManager = createMemoryManager();
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2M));
    EXPECT_TRUE(isAligned(allocation->getUnderlyingBufferSize(), MemoryConstants::pageSize2M));

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(allocation->getGpuAddress(), bo->peekAddress());
    EXPECT_TRUE(isAligned(bo->peekAddress(), MemoryConstants::pageSize2M));
    EXPECT_TRUE(isAligned(bo->peekSize(), MemoryConstants::pageSize2M));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, Given2MBLocalMemAlignmentEnabledWhenAllocating2MBPageTypeInDevicePoolThenAllocationIs2MBAligned) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocType = static_cast<AllocationType>(i);

        if (!GraphicsAllocation::is2MBPageAllocationType(allocType)) {
            continue;
        }

        const auto requestedSize = MemoryConstants::pageSize;
        const auto expectedAlignedSize = alignUp(requestedSize, MemoryConstants::pageSize2M);

        AllocationData allocationData;
        allocationData.allFlags = 0;
        allocationData.flags.allocateMemory = true;
        allocationData.rootDeviceIndex = rootDeviceIndex;
        allocationData.type = allocType;
        allocationData.flags.resource48Bit = true;
        allocationData.size = requestedSize;

        MemoryManager::AllocationStatus allocationStatus;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::heapStandard2MB));
        EXPECT_EQ(expectedAlignedSize, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(expectedAlignedSize, allocation->getReservedAddressSize());
        EXPECT_TRUE(isAligned<MemoryConstants::pageSize2M>(allocation->getGpuAddress()));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    auto sizeAligned = alignUp(allocData.size, alignmentSize);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_EQ(0u, gmmResourceParams->Flags.Info.NonLocalOnly);

    EXPECT_EQ(RESOURCE_BUFFER, gmmResourceParams->Type);
    EXPECT_EQ(sizeAligned, gmmResourceParams->BaseWidth64);
    EXPECT_NE(nullptr, gmm->gmmResourceInfo->peekHandle());
    EXPECT_NE(0u, gmm->gmmResourceInfo->getHAlign());

    auto gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0u, gpuAddress);

    auto heap = productHelper.is2MBLocalMemAlignmentEnabled() ? HeapIndex::heapStandard2MB : HeapIndex::heapStandard64KB;
    if (memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        heap = HeapIndex::heapExtended;
    }

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(heap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(heap)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_EQ(allocData.storageInfo.cloningOfPageTables, allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(allocData.storageInfo.tileInstanced, allocation->storageInfo.tileInstanced);
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPatIndexProgrammingAllowedWhenCreatingAllocationThenSetValidPatIndex) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    ASSERT_NE(nullptr, drmAllocation->getBO());
    auto &productHelper = this->device->getProductHelper();
    auto isVmBindPatIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();

    EXPECT_EQ(isVmBindPatIndexProgrammingSupported, mock->isVmBindPatIndexProgrammingSupported());

    if (isVmBindPatIndexProgrammingSupported) {
        auto expectedIndex = mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false);

        EXPECT_NE(CommonConstants::unsupportedPatIndex, drmAllocation->getBO()->peekPatIndex());
        EXPECT_EQ(expectedIndex, drmAllocation->getBO()->peekPatIndex());
    } else {
        EXPECT_EQ(CommonConstants::unsupportedPatIndex, drmAllocation->getBO()->peekPatIndex());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenCompressedAndCacheableAllocationWhenQueryingPatIndexThenPassCorrectParams) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 1;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    ASSERT_NE(nullptr, drmAllocation->getBO());
    auto &productHelper = this->device->getProductHelper();
    auto isVmBindPatIndexProgrammingSupported = productHelper.isVmBindPatIndexProgrammingSupported();

    if (isVmBindPatIndexProgrammingSupported) {
        auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
        auto gmm = allocation->getDefaultGmm();

        {
            gmm->setCompressionEnabled(true);
            static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = 1;

            mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false);

            EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
            EXPECT_TRUE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
        }

        {
            gmm->setCompressionEnabled(false);
            static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = 0;

            mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false);

            EXPECT_FALSE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
            EXPECT_FALSE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
        }
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForImageThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::image;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_EQ(0u, gmmResourceParams->Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    auto sizeAligned = alignUp(allocData.imgInfo->size, alignmentSize);
    EXPECT_NE(0u, gpuAddress);

    auto heap = productHelper.is2MBLocalMemAlignmentEnabled() ? HeapIndex::heapStandard2MB : HeapIndex::heapStandard64KB;
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(heap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(heap)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_EQ(allocData.storageInfo.cloningOfPageTables, allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(allocData.storageInfo.tileInstanced, allocation->storageInfo.tileInstanced);
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocatioInDevicePoolIsAllocatednForKernelIsaThenLocalMemoryAllocationIsReturnedFromInternalHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    auto sizeAligned = alignUp(allocData.size, alignmentSize);

    AllocationType isaTypes[] = {AllocationType::kernelIsa, AllocationType::kernelIsaInternal};
    for (uint32_t i = 0; i < 2; i++) {
        allocData.type = isaTypes[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

        auto gpuAddress = allocation->getGpuAddress();
        auto gmmHelper = device->getGmmHelper();
        EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
        EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
        EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), allocation->getGpuBaseAddress());
        EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
        EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto bo = drmAllocation->getBO();
        EXPECT_NE(nullptr, bo);
        EXPECT_EQ(gpuAddress, bo->peekAddress());
        EXPECT_EQ(sizeAligned, bo->peekSize());

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUnsupportedTypeWhenAllocatingInDevicePoolThenRetryInNonDevicePoolStatusAndNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType unsupportedTypes[] = {AllocationType::sharedResourceCopy};

    for (auto unsupportedType : unsupportedTypes) {
        allocData.type = unsupportedType;

        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationsThatAreAlignedToPowerOf2InSizeAndAreGreaterThen8GBThenTheyAreAlignedToPreviousPowerOfTwoForGpuVirtualAddress) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }
    auto &productHelper = this->device->getProductHelper();

    auto size = 8 * MemoryConstants::gigaByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    EXPECT_TRUE(allocation->getGpuAddress() % size == 0u);

    size = 8 * MemoryConstants::gigaByte + MemoryConstants::pageSize64k;
    size_t expectedSize{};

    if (productHelper.is2MBLocalMemAlignmentEnabled()) {
        expectedSize = alignUp(size, MemoryConstants::pageSize2M);
    } else {
        expectedSize = alignUp(size, MemoryConstants::pageSize64k);
    }

    allocData.size = size;
    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(expectedSize, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(expectedSize, static_cast<DrmAllocation *>(allocation2)->getBO()->peekSize());
    EXPECT_TRUE(allocation2->getGpuAddress() % MemoryConstants::pageSize2M == 0u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDebugVariableToDisableAddressAlignmentWhenAllocationsAreMadeTheyAreAlignedTo2MB) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.UseHighAlignmentForHeapExtended.set(0);

    auto size = 16 * MemoryConstants::megaByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    EXPECT_TRUE(allocation->getGpuAddress() % MemoryConstants::pageSize2M == 0u);

    size = 32 * MemoryConstants::megaByte;
    allocData.size = size;
    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(allocData.size, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation2)->getBO()->peekSize());
    EXPECT_TRUE(allocation2->getGpuAddress() % MemoryConstants::pageSize2M == 0u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenEnabled2MBSizeAlignmentWhenAllocatingLargeImageAllocationThenUseProperAlignment) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;
    imgDesc.imageWidth = 2 * MemoryConstants::megaByte + 1;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::image;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_EQ(0u, gmmResourceParams->Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAligned = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize2M);

    EXPECT_NE(0u, gpuAddress);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard2MB)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard2MB)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

struct DrmMemoryManagerToTestCopyMemoryToAllocationBanks : public DrmMemoryManager {
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks(ExecutionEnvironment &executionEnvironment, size_t lockableLocalMemorySize)
        : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            if (static_cast<uint32_t>(bo->peekHandle()) < lockedLocalMemory.size()) {
                lockedLocalMemory[bo->peekHandle()].reset(new uint8_t[lockedLocalMemorySize]);
                return lockedLocalMemory[bo->peekHandle()].get();
            }
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    size_t lockedLocalMemorySize = 0;
};

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationOnSpecificMemoryBanksThenAllocationIsFilledWithCorrectDataOnSpecificBanks) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManager(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::workPartitionSurface, MemoryPool::localMemory);

    mockAllocation.storageInfo.memoryBanks = 0b1110;
    DeviceBitfield memoryBanksToCopy = 0b1010;
    mockAllocation.bufferObjects.clear();

    for (auto index = 1u; index < 4; index++) {
        drmMemoryManager.lockedLocalMemory[index].reset();
        mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, index, sourceAllocationSize, 3));
    }

    auto ret = drmMemoryManager.copyMemoryToAllocationBanks(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size(), memoryBanksToCopy);
    EXPECT_TRUE(ret);

    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[0].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[1].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[2].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[1].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[3].get(), offset), dataToCopy.data(), dataToCopy.size()));

    for (auto bufferObject : mockAllocation.bufferObjects) {
        delete bufferObject;
    }
}

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationAndThereIsNoUnderlyingBufferThenAllocationIsFilledWithCorrectDataOnAllBanks) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManager(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::workPartitionSurface, MemoryPool::localMemory);

    mockAllocation.storageInfo.memoryBanks = 0b1110;
    mockAllocation.bufferObjects.clear();

    for (auto index = 1u; index < 4; index++) {
        drmMemoryManager.lockedLocalMemory[index].reset();
        mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, index, sourceAllocationSize, 3));
    }

    auto ret = drmMemoryManager.copyMemoryToAllocation(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[0].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[1].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[2].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[1].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[2].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[3].get(), offset), dataToCopy.data(), dataToCopy.size()));

    for (auto bufferObject : mockAllocation.bufferObjects) {
        delete bufferObject;
    }
}

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenCopyMemoryToSystemMemoryAllocationAndThereIsNoUnderlyingBufferThenAllocationIsFilledWithTheFirstBufferObject) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManager(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::constantSurface, MemoryPool::systemCpuInaccessible);

    mockAllocation.storageInfo.memoryBanks = 0b0;
    mockAllocation.bufferObjects.clear();

    drmMemoryManager.lockedLocalMemory[0].reset();
    mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, 0, sourceAllocationSize, 3));

    auto ret = drmMemoryManager.copyMemoryToAllocation(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[0].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[1].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[2].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[0].get(), offset), dataToCopy.data(), dataToCopy.size()));

    for (auto bufferObject : mockAllocation.bufferObjects) {
        delete bufferObject;
    }
}

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenMemsetAllocationOnSpecificMemoryBanksThenAllocationIsFilledWithCorrectDataOnSpecificBanks) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManager(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::workPartitionSurface, MemoryPool::localMemory);

    mockAllocation.storageInfo.memoryBanks = 0b1110;
    DeviceBitfield memoryBanksToCopy = 0b1010;
    mockAllocation.bufferObjects.clear();

    for (auto index = 1u; index < 4; index++) {
        drmMemoryManager.lockedLocalMemory[index].reset();
        mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, index, sourceAllocationSize, 3));
    }

    auto ret = drmMemoryManager.memsetAllocationBanks(&mockAllocation, offset, dataToCopy.front(), dataToCopy.size(), memoryBanksToCopy);
    EXPECT_TRUE(ret);

    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[0].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[1].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[2].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[1].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[3].get(), offset), dataToCopy.data(), dataToCopy.size()));

    for (auto bufferObject : mockAllocation.bufferObjects) {
        delete bufferObject;
    }
}

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenMemsetAllocationAndThereIsNoUnderlyingBufferThenAllocationIsFilledWithCorrectDataOnAllBanks) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManager(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::workPartitionSurface, MemoryPool::localMemory);

    mockAllocation.storageInfo.memoryBanks = 0b1110;
    mockAllocation.bufferObjects.clear();

    for (auto index = 1u; index < 4; index++) {
        drmMemoryManager.lockedLocalMemory[index].reset();
        mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, index, sourceAllocationSize, 3));
    }

    auto ret = drmMemoryManager.memsetAllocation(&mockAllocation, offset, dataToCopy.front(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[0].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[1].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[2].get());
    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[1].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[2].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[3].get(), offset), dataToCopy.data(), dataToCopy.size()));

    for (auto bufferObject : mockAllocation.bufferObjects) {
        delete bufferObject;
    }
}

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenMemsetSystemMemoryAllocationAndThereIsNoUnderlyingBufferThenAllocationIsFilledWithTheFirstBufferObject) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManager(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(0u, AllocationType::constantSurface, MemoryPool::systemCpuInaccessible);

    mockAllocation.storageInfo.memoryBanks = 0b0;
    mockAllocation.bufferObjects.clear();

    drmMemoryManager.lockedLocalMemory[0].reset();
    mockAllocation.bufferObjects.push_back(new BufferObject(0u, drm, 3, 0, sourceAllocationSize, 3));

    auto ret = drmMemoryManager.memsetAllocation(&mockAllocation, offset, dataToCopy.front(), dataToCopy.size());
    EXPECT_TRUE(ret);

    ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[0].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[1].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[2].get());
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[0].get(), offset), dataToCopy.data(), dataToCopy.size()));

    for (auto bufferObject : mockAllocation.bufferObjects) {
        delete bufferObject;
    }
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectSucceedsThenReturnTrueAndCorrectOffset, IsAtMostXeCore) {
    mock->ioctlExpected.gemMmapOffset = 1;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);
    mock->mmapOffsetExpected = 21;

    auto drm = bo.peekDrm();
    auto ioctlHelper = drm->getIoctlHelper();

    uint64_t offset = 0;
    auto ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, 0, offset);

    EXPECT_TRUE(ret);
    EXPECT_EQ(21u, offset);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectFailsThenReturnFalse, IsAtMostXeCore) {
    mock->ioctlExpected.gemMmapOffset = 2;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);
    mock->failOnMmapOffset = true;

    auto drm = bo.peekDrm();
    auto ioctlHelper = drm->getIoctlHelper();

    uint64_t offset = 0;
    auto ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, 0, offset);

    EXPECT_FALSE(ret);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectFailsThenReturnFalseTestErrorDescription, IsAtMostXeCore) {
    mock->ioctlExpected.gemMmapOffset = 2;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);
    mock->failOnMmapOffset = true;

    // To set the error value used to create the debug string in retrieveMmapOffsetForBufferObject()
    mock->errnoValue = -2;

    auto drm = bo.peekDrm();
    auto ioctlHelper = drm->getIoctlHelper();

    uint64_t offset = 0;
    auto ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, 0, offset);

    const char *systemErrorDescription = nullptr;
    executionEnvironment->getErrorDescription(&systemErrorDescription);

    char expectedErrorDescription[256];
    snprintf(expectedErrorDescription, 256, "ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET) failed with %d. errno=%d(%s)\n", -1, mock->getErrno(), strerror(mock->getErrno()));

    EXPECT_STREQ(expectedErrorDescription, systemErrorDescription);

    EXPECT_FALSE(ret);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectIsCalledForLocalMemoryThenApplyCorrectFlags, IsAtMostXeCore) {
    mock->ioctlExpected.gemMmapOffset = 5;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    auto drm = bo.peekDrm();
    auto ioctlHelper = drm->getIoctlHelper();

    uint64_t offset = 0;
    auto ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, 0, offset);

    EXPECT_TRUE(ret);
    EXPECT_EQ(4u, mock->mmapOffsetFlags);

    mock->failOnMmapOffset = true;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, flags, offset);

        EXPECT_FALSE(ret);
        EXPECT_EQ(flags, mock->mmapOffsetFlags);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenMakeResidentBeforeLockNeededWhenCreateSharedUnifiedMemoryAllocationThenRequireImmediateBindingIsSetAndBindIsCalled) {
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemCreateExt = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mockIoctlHelper->isDeferBackingEnabledForAllocationResult = true;
    mockIoctlHelper->callBaseVmAdviseAtomicAttribute = false;
    mockIoctlHelper->vmAdviseAtomicAttribute = std::nullopt;

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper.reset(mockIoctlHelper);

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.reset(
        new DrmMemoryOperationsHandlerBind(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get(), 0));

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::unifiedSharedMemory;
    allocationData.storageInfo.subDeviceBitfield = 0x1;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    ASSERT_NE(nullptr, sharedUSM);

    auto osContext = device->getDefaultEngine().osContext;
    EXPECT_TRUE(sharedUSM->isAlwaysResident(osContext->getContextId()));

    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectIsCalledForSystemMemoryThenApplyCorrectFlags, IsAtMostXeCore) {
    mock->ioctlExpected.gemMmapOffset = 8;
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    uint64_t offset = 0;
    bool ret = false;
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    bo.setBOType(BufferObject::BOType::legacy);

    auto drm = bo.peekDrm();
    auto ioctlHelper = drm->getIoctlHelper();

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, flags, offset);

        EXPECT_TRUE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WB), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }

    bo.setBOType(BufferObject::BOType::coherent);
    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, flags, offset);

        EXPECT_TRUE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WB), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }

    bo.setBOType(BufferObject::BOType::nonCoherent);
    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, flags, offset);

        EXPECT_TRUE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WC), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }

    mock->failOnMmapOffset = true;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = ioctlHelper->retrieveMmapOffsetForBufferObject(bo, flags, offset);

        EXPECT_FALSE(ret);
        if (productHelper.useGemCreateExtInAllocateMemoryByKMD()) {
            EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WC), mock->mmapOffsetFlags);
        } else {
            EXPECT_EQ(flags, mock->mmapOffsetFlags);
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmWhenGetBOTypeFromPatIndexIsCalledThenReturnCorrectBOType) {
    const bool isPatIndexSupported = memoryManager->getGmmHelper(mockRootDeviceIndex)->getRootDeviceEnvironment().getProductHelper().isVmBindPatIndexProgrammingSupported();
    if (!isPatIndexSupported) {
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(0, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(1, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(2, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(3, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::legacy, memoryManager->getBOTypeFromPatIndex(4, isPatIndexSupported));
    } else {
        EXPECT_EQ(BufferObject::BOType::nonCoherent, memoryManager->getBOTypeFromPatIndex(0, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::nonCoherent, memoryManager->getBOTypeFromPatIndex(1, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::nonCoherent, memoryManager->getBOTypeFromPatIndex(2, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::coherent, memoryManager->getBOTypeFromPatIndex(3, isPatIndexSupported));
        EXPECT_EQ(BufferObject::BOType::coherent, memoryManager->getBOTypeFromPatIndex(4, isPatIndexSupported));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenEligbleAllocationTypeWhenCheckingAllocationEligbleForCompletionFenceThenReturnTrue) {
    constexpr size_t arraySize = 5;

    AllocationType validAllocations[arraySize] = {
        AllocationType::commandBuffer,
        AllocationType::ringBuffer,
        AllocationType::deferredTasksList,
        AllocationType::semaphoreBuffer,
        AllocationType::tagBuffer};

    for (size_t i = 0; i < arraySize; i++) {
        EXPECT_TRUE(memoryManager->allocationTypeForCompletionFence(validAllocations[i]));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNotEligbleAllocationTypeWhenCheckingAllocationEligbleForCompletionFenceThenReturnFalse) {
    AllocationType invalidAllocations[] = {
        AllocationType::bufferHostMemory,
        AllocationType::constantSurface,
        AllocationType::fillPattern,
        AllocationType::globalSurface};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_FALSE(memoryManager->allocationTypeForCompletionFence(invalidAllocations[i]));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, GivenNotEligbleAllocationTypeAndDebugFlagOverridingWhenCheckingAllocationEligbleForCompletionFenceThenReturnTrue) {
    DebugManagerStateRestore dbgState;
    debugManager.flags.UseDrmCompletionFenceForAllAllocations.set(1);

    AllocationType invalidAllocations[] = {
        AllocationType::bufferHostMemory,
        AllocationType::constantSurface,
        AllocationType::fillPattern,
        AllocationType::globalSurface};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(memoryManager->allocationTypeForCompletionFence(invalidAllocations[i]));
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfUsedAndEligbleAllocationThenCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});
    auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    uint64_t expectedFenceAddress = castToUint64(const_cast<TagAddressType *>(engine.commandStreamReceiver->getTagAddress())) + TagAllocationLayout::completionFenceOffset;
    constexpr uint64_t expectedValue = 2;

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedFenceAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithDifferentScratchPageOptionsWhenHandleFenceCompletionThenCallResetStatsOnlyWithScratchPageDisabledAndProperFaultCheckThreshold) {
    mock->ioctlExpected.total = -1;

    DebugManagerStateRestore dbgStateRestore;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    for (int err : {EIO, ETIME}) {
        for (bool disableScratchPage : {false, true}) {
            for (int gpuFaultCheckThreshold : {0, 10}) {
                debugManager.flags.DisableScratchPages.set(disableScratchPage);
                debugManager.flags.GpuFaultCheckThreshold.set(gpuFaultCheckThreshold);

                mock->configureScratchPagePolicy();
                mock->configureGpuFaultCheckThreshold();

                mock->ioctlCnt.reset();
                mock->waitUserFenceCall.called = 0u;
                mock->checkResetStatusCalled = 0u;

                mock->waitUserFenceCall.failOnWaitUserFence = true;
                mock->errnoValue = err;

                auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});
                auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
                allocation->updateTaskCount(2, engine.osContext->getContextId());

                uint64_t expectedFenceAddress = castToUint64(const_cast<TagAddressType *>(engine.commandStreamReceiver->getTagAddress())) + TagAllocationLayout::completionFenceOffset;
                constexpr uint64_t expectedValue = 2;

                memoryManager->handleFenceCompletion(allocation);

                EXPECT_EQ(1u, mock->waitUserFenceCall.called);
                EXPECT_EQ(expectedFenceAddress, mock->waitUserFenceCall.address);
                EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);
                if (err == EIO && disableScratchPage && gpuFaultCheckThreshold != 0) {
                    EXPECT_EQ(1u, mock->checkResetStatusCalled);
                } else {
                    EXPECT_EQ(0u, mock->checkResetStatusCalled);
                }

                memoryManager->freeGraphicsMemory(allocation);
            }
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfNotUsedAndEligbleAllocationThenDoNotCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfUsedAndNotEligbleAllocationThenDoNotCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::globalSurface});
    auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionAndTagAddressIsNullThenDoNotCallWaitUserFence) {
    mock->ioctlExpected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::commandBuffer});
    auto engine = memoryManager->getRegisteredEngines(rootDeviceIndex)[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    auto testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
    auto backupTagAddress = testCsr->tagAddress;
    testCsr->tagAddress = nullptr;

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    testCsr->tagAddress = backupTagAddress;

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSingleSubDevicesBitfieldWhenAllocatingSbaTrackingBufferThenSingleHostAllocationReturned) {
    mock->ioctlExpected.total = -1;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, false,
                                         0b0001};

    const uint64_t gpuAddresses[] = {0, 0x12340000};

    for (auto gpuAddress : gpuAddresses) {
        properties.gpuAddress = gpuAddress;

        auto sbaBuffer = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));

        EXPECT_NE(nullptr, sbaBuffer);

        EXPECT_EQ(MemoryPool::system4KBPages, sbaBuffer->getMemoryPool());
        EXPECT_EQ(1u, sbaBuffer->getNumGmms());

        EXPECT_NE(nullptr, sbaBuffer->getUnderlyingBuffer());
        EXPECT_EQ(MemoryConstants::pageSize, sbaBuffer->getUnderlyingBufferSize());

        auto &bos = sbaBuffer->getBOs();

        EXPECT_NE(nullptr, bos[0]);
        EXPECT_EQ(nullptr, bos[1]);
        EXPECT_EQ(nullptr, bos[2]);
        EXPECT_EQ(nullptr, bos[3]);

        if (gpuAddress != 0) {
            EXPECT_EQ(gpuAddress, sbaBuffer->getGpuAddress());
            EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
        } else {
            EXPECT_NE(0u, sbaBuffer->getGpuAddress());
        }

        memoryManager->freeGraphicsMemory(sbaBuffer);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMakeBosResidentErrorWhenRegisteringMemoryAllocationThenErrorIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(1);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.vmIdToCreate = 1;
    drm.createVirtualMemoryAddressSpace(1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation.makeBOsResidentResult = ENOSPC;

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, memoryManager->registerSysMemAlloc(&allocation));

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, memoryManager->registerLocalMemAlloc(&allocation, rootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMakeBosResidentSuccessWhenRegisteringMemoryAllocationThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(1);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(0));
    drm.vmIdToCreate = 1;
    drm.createVirtualMemoryAddressSpace(1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation.makeBOsResidentResult = 0;

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->registerSysMemAlloc(&allocation));

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->registerLocalMemAlloc(&allocation, 0));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMakeBosResidentErrorWhenRegisteringDeferBackedLocalMemoryAllocationThenUnregisteringItKeepsDeferBackedSizeOfOtherAllocations) {
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeEachAllocationResident.set(1);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.vmIdToCreate = 1;
    drm.createVirtualMemoryAddressSpace(1);

    MockBufferObject registeredBo(rootDeviceIndex, &drm, 3, 0, 500u, 1);
    registeredBo.setDeferBackingUsed(true);
    MockDrmAllocation registeredAllocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    registeredAllocation.bufferObjects[0] = &registeredBo;
    registeredAllocation.setSize(500u);
    registeredAllocation.makeBOsResidentResult = 0;

    MockBufferObject failedBo(rootDeviceIndex, &drm, 3, 0, 1000u, 1);
    failedBo.setDeferBackingUsed(true);
    MockDrmAllocation failedAllocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    failedAllocation.bufferObjects[0] = &failedBo;
    failedAllocation.setSize(1000u);
    failedAllocation.makeBOsResidentResult = ENOSPC;

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->registerLocalMemAlloc(&registeredAllocation, rootDeviceIndex));
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, memoryManager->registerLocalMemAlloc(&failedAllocation, rootDeviceIndex));
    EXPECT_EQ(500u, memoryManager->deferBackedLocalMemAllocsSize);

    memoryManager->localMemAllocsSize[rootDeviceIndex] += 1000u;
    memoryManager->unregisterAllocation(&failedAllocation);
    EXPECT_EQ(500u, memoryManager->deferBackedLocalMemAllocsSize);

    memoryManager->unregisterAllocation(&registeredAllocation);
    EXPECT_EQ(0u, memoryManager->deferBackedLocalMemAllocsSize);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithKnownAddressAtIndex1ThenAddressFromGfxPartitionIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(1);
    uint32_t rootDeviceIndexReserved = 0;
    auto gmmHelper = memoryManager->getGmmHelper(1);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    uint64_t requiredAddr = addressRange.address;
    memoryManager->freeGpuAddress(addressRange, 1);
    addressRange = memoryManager->reserveGpuAddressOnHeap(requiredAddr, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_EQ(addressRange.address, requiredAddr);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);

    addressRange = memoryManager->reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    requiredAddr = addressRange.address;
    memoryManager->freeGpuAddress(addressRange, 1);
    addressRange = memoryManager->reserveGpuAddress(requiredAddr, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_EQ(addressRange.address, requiredAddr);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedWithSizeGreaterThanSizeThreshold4MBInAKnownRegionThenRequiredAddressIsFoundAndUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(1);
    uint32_t rootDeviceIndexReserved = 0;
    auto gmmHelper = memoryManager->getGmmHelper(1);

    size_t sizeToAllocate = 128 * MemoryConstants::pageSize64k; // 8MB = 128 * 64KB
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, sizeToAllocate, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    uint64_t requiredAddr = addressRange.address + sizeToAllocate;
    memoryManager->freeGpuAddress(addressRange, 1);
    addressRange = memoryManager->reserveGpuAddressOnHeap(requiredAddr, sizeToAllocate, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 1u);
    EXPECT_EQ(addressRange.address, requiredAddr);
    EXPECT_LE(memoryManager->getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 1);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressReservationIsAttemptedOnHeapExtendedWithCustomAlignmentThenRequiredAddressIsFoundAndUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto gfxPartition = memoryManager->getGfxPartition(0);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    if (!gfxPartition->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    gfxPartition->heapInitWithAllocationAlignment(HeapIndex::heapExtended, 0x0, 0x800200400000, MemoryConstants::pageSize);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;

    HeapIndex heap = HeapIndex::heapStandard;
    size_t size = 128 * MemoryConstants::pageSize64k; // 8MB = 128 * 64KB
    auto alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    auto addressRange = memoryManager->reserveGpuAddressOnHeap(0ull, size, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_NE(addressRange.address, 0ull);
    EXPECT_NE(addressRange.size, 0ull);

    heap = HeapIndex::heapStandard;
    size = 8 * MemoryConstants::gigaByte;
    alignment = memoryManager->selectAlignmentAndHeap(size, &heap);
    uint64_t requiredAddr = alignUp(addressRange.address + size, alignment);
    auto addressRange2 = memoryManager->reserveGpuAddressOnHeap(requiredAddr, size, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(heap, HeapIndex::heapExtended);
    EXPECT_EQ(size, alignment);
    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_EQ(addressRange2.address, requiredAddr);

    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExtended), gmmHelper->decanonize(addressRange2.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended), gmmHelper->decanonize(addressRange2.address));

    memoryManager->freeGpuAddress(addressRange, 0);
    memoryManager->freeGpuAddress(addressRange2, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCpuAddressReservationIsAttemptedThenCorrectAddressRangesAreReturned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);

    auto addressRange = memoryManager->reserveCpuAddress(0, 0);
    EXPECT_EQ(0u, addressRange.address);
    EXPECT_EQ(0u, addressRange.size);

    addressRange = memoryManager->reserveCpuAddress(0, MemoryConstants::pageSize);
    EXPECT_NE(0u, addressRange.address);
    EXPECT_EQ(MemoryConstants::pageSize, addressRange.size);
    memoryManager->freeCpuAddress(addressRange);

    addressRange = memoryManager->reserveCpuAddress(MemoryConstants::pageSize * 1234, MemoryConstants::pageSize);
    EXPECT_NE(0u, addressRange.address);
    EXPECT_EQ(MemoryConstants::pageSize, addressRange.size);
    memoryManager->freeCpuAddress(addressRange);

    VariableBackup<bool> backup(&SysCalls::failMmap, true);
    addressRange = memoryManager->reserveCpuAddress(0, 0);
    EXPECT_EQ(0u, addressRange.address);
    EXPECT_EQ(0u, addressRange.size);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuWhenAllocatingHostUSMThenAddressFromExtendedHeapIsPassedAsHintAndSetAsGpuAddressAndReservedAddress) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    AllocationProperties allocationProperties(mockRootDeviceIndex, MemoryConstants::cacheLineSize, AllocationType::svmCpu, {});
    allocationProperties.flags.isUSMHostAllocation = true;
    auto hostUSM = memoryManager->allocateGraphicsMemoryInPreferredPool(allocationProperties, nullptr);
    EXPECT_NE(nullptr, hostUSM);

    EXPECT_EQ(2u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->productHelper;
    auto maxDiff = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    EXPECT_LE(hostUSM->getGpuAddress(), gpuAddress + maxDiff);
    EXPECT_GE(hostUSM->getGpuAddress(), gpuAddress);
    EXPECT_LE(reinterpret_cast<uint64_t>(hostUSM->getReservedAddressPtr()), gpuAddress + maxDiff);
    EXPECT_GE(reinterpret_cast<uint64_t>(hostUSM->getReservedAddressPtr()), gpuAddress);
    memoryManager->freeGraphicsMemory(hostUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given48bAddressSpaceCpuAnd57bGpuWhenAllocatingHostUSMThenAddressFromExtendedHeapIsPassedAsHintAndThenIgnored) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, false);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    AllocationProperties allocationProperties(mockRootDeviceIndex, MemoryConstants::cacheLineSize, AllocationType::svmCpu, {});
    allocationProperties.flags.isUSMHostAllocation = true;
    auto hostUSM = memoryManager->allocateGraphicsMemoryInPreferredPool(allocationProperties, nullptr);
    EXPECT_NE(nullptr, hostUSM);

    EXPECT_EQ(1u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_NE(hostUSM->getGpuAddress(), gpuAddress);
    memoryManager->freeGraphicsMemory(hostUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuAndDisabledHeapExtendedUsageForUsmHostWhenAllocatingHostUSMThenAddressFromExtendedHeapIsNotUsed) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateHostAllocationsInHeapExtendedHost.set(false);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    AllocationProperties allocationProperties(mockRootDeviceIndex, MemoryConstants::cacheLineSize, AllocationType::svmCpu, {});
    allocationProperties.flags.isUSMHostAllocation = true;
    auto hostUSM = memoryManager->allocateGraphicsMemoryInPreferredPool(allocationProperties, nullptr);
    EXPECT_NE(nullptr, hostUSM);

    EXPECT_EQ(0u, SysCalls::mmapCapturedExtendedPointers.size());
    memoryManager->freeGraphicsMemory(hostUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuWhenAllocatingSharedUSMThenAddressFromExtendedHeapIsPassedAsHintAndSetAsGpuAddressAndReservedAddress) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.set(true);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    EXPECT_EQ(2u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_EQ(sharedUSM->getGpuAddress(), gpuAddress);
    EXPECT_EQ(sharedUSM->getReservedAddressPtr(), reinterpret_cast<void *>(gpuAddress));
    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given48bAddressSpaceCpuAnd57bGpuWhenAllocatingSharedUSMThenAddressFromExtendedHeapIsPassedAsHintAndThenIgnored) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.set(true);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, false);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    EXPECT_EQ(1u, SysCalls::mmapCapturedExtendedPointers.size());
    auto gpuAddress = reinterpret_cast<uint64_t>(SysCalls::mmapCapturedExtendedPointers[0]);
    SysCalls::mmapCapturedExtendedPointers.clear();
    auto gmmHelper = memoryManager->getGmmHelper(mockRootDeviceIndex);
    EXPECT_LE(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapBase(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));
    EXPECT_GT(memoryManager->getGfxPartition(mockRootDeviceIndex)->getHeapLimit(HeapIndex::heapExtendedHost), gmmHelper->decanonize(gpuAddress));

    EXPECT_NE(sharedUSM->getGpuAddress(), gpuAddress);
    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given57bAddressSpaceCpuAndGpuWhenAllocating48bResourceSharedUSMThenAddressFromExtendedHeapIsNotUsed) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtendedHost, maxNBitValue(48) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsInHeapExtendedHost.set(true);
    VariableBackup<bool> backupCaptureExtendedPointers(&SysCalls::mmapCaptureExtendedPointers, true);
    VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);
    SysCalls::mmapCapturedExtendedPointers.clear();
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.useMmapObject = true;
    allocationData.flags.resource48Bit = true;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    EXPECT_EQ(0u, SysCalls::mmapCapturedExtendedPointers.size());

    EXPECT_LT(sharedUSM->getGpuAddress(), maxNBitValue(48));
    EXPECT_EQ(sharedUSM->getReservedAddressPtr(), nullptr);
    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugVariableToToggleGpuVaBitsWhenAllocatingResourceInHeapExtendedThenSpecificBitIsToggled) {
    if (defaultHwInfo->capabilityTable.gpuAddressSpace < maxNBitValue(57)) {
        GTEST_SKIP();
    }
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapExtended, maxNBitValue(56) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memoryManager->gfxPartitions[1] = std::move(mockGfxPartition);

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(rootDeviceIndex));
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);
    memoryManager->localMemorySupported[rootDeviceIndex] = true;

    mock->ioctlExpected.gemCreateExt = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 3;

    DebugManagerStateRestore restorer;
    EXPECT_EQ(4u, static_cast<uint32_t>(AllocationType::constantSurface));
    EXPECT_EQ(7u, static_cast<uint32_t>(AllocationType::globalSurface));
    debugManager.flags.ToggleBitIn57GpuVa.set("4:55,7:32");

    auto size = MemoryConstants::kiloByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    {
        allocData.type = AllocationType::buffer;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_FALSE(isBitSet(gpuVA, 55));
        EXPECT_TRUE(isBitSet(gpuVA, 32));

        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        allocData.type = AllocationType::constantSurface;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_TRUE(isBitSet(gpuVA, 55));
        EXPECT_TRUE(isBitSet(gpuVA, 32));

        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        allocData.type = AllocationType::globalSurface;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        ASSERT_NE(nullptr, allocation);

        auto gpuVA = allocation->getGpuAddress();

        EXPECT_TRUE(isBitSet(gpuVA, 56));
        EXPECT_FALSE(isBitSet(gpuVA, 55));
        EXPECT_FALSE(isBitSet(gpuVA, 32));

        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenIsCompressionSupportedForShareableThenReturnCorrectValue) {
    EXPECT_FALSE(memoryManager->isCompressionSupportedForShareable(true));
    EXPECT_TRUE(memoryManager->isCompressionSupportedForShareable(false));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenVmAdviseAtomicAttributeEqualZeroWhenCreateSharedUnifiedMemoryAllocationIsCalledThenNullptrReturned) {
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;

    mockIoctlHelper->callBaseVmAdviseAtomicAttribute = false;
    mockIoctlHelper->vmAdviseAtomicAttribute = 0;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_EQ(nullptr, sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenVmAdviseAtomicAttributeNotPresentWhenCreateSharedUnifiedMemoryAllocationIsCalledThenAllocationIsCreatedSuccessfully) {
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemCreateExt = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    allocationData.alignment = MemoryConstants::pageSize;

    mockIoctlHelper->callBaseVmAdviseAtomicAttribute = false;
    mockIoctlHelper->vmAdviseAtomicAttribute = std::nullopt;

    auto sharedUSM = memoryManager->createSharedUnifiedMemoryAllocation(allocationData);
    EXPECT_NE(nullptr, sharedUSM);

    memoryManager->freeGraphicsMemory(sharedUSM);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenGfxPartitionWhenReleasedAndReinitializedThenNewGfxPartitionIsCorrect) {

    auto gfxPartition = memoryManager->getGfxPartition(0);

    auto heapExternal = gfxPartition->getHeapBase(HeapIndex::heapExternal);
    auto heapStandard = gfxPartition->getHeapBase(HeapIndex::heapStandard);
    auto heapStandard64KB = gfxPartition->getHeapBase(HeapIndex::heapStandard64KB);
    auto heapSvm = gfxPartition->getHeapBase(HeapIndex::heapSvm);
    auto heapExtended = gfxPartition->getHeapBase(HeapIndex::heapExtended);
    auto heapExternalFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapExternalFrontWindow);
    auto heapExternalDeviceFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapExternalDeviceFrontWindow);
    auto heapInternalFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapInternalFrontWindow);
    auto heapInternalDeviceFrontWindow = gfxPartition->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow);

    memoryManager->releaseDeviceSpecificGfxPartition(0);
    EXPECT_EQ(nullptr, memoryManager->getGfxPartition(0));
    memoryManager->reInitDeviceSpecificGfxPartition(0);

    EXPECT_NE(nullptr, memoryManager->getGfxPartition(0));

    gfxPartition = memoryManager->getGfxPartition(0);

    auto heapExternal2 = gfxPartition->getHeapBase(HeapIndex::heapExternal);
    auto heapStandard2 = gfxPartition->getHeapBase(HeapIndex::heapStandard);
    auto heapStandard64KB2 = gfxPartition->getHeapBase(HeapIndex::heapStandard64KB);
    auto heapSvm2 = gfxPartition->getHeapBase(HeapIndex::heapSvm);
    auto heapExtended2 = gfxPartition->getHeapBase(HeapIndex::heapExtended);
    auto heapExternalFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapExternalFrontWindow);
    auto heapExternalDeviceFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapExternalDeviceFrontWindow);
    auto heapInternalFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapInternalFrontWindow);
    auto heapInternalDeviceFrontWindow2 = gfxPartition->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow);

    EXPECT_EQ(heapExternal, heapExternal2);
    EXPECT_EQ(heapStandard, heapStandard2);
    EXPECT_EQ(heapStandard64KB, heapStandard64KB2);
    EXPECT_EQ(heapSvm, heapSvm2);
    EXPECT_EQ(heapExtended, heapExtended2);
    EXPECT_EQ(heapExternalFrontWindow, heapExternalFrontWindow2);
    EXPECT_EQ(heapExternalDeviceFrontWindow, heapExternalDeviceFrontWindow2);
    EXPECT_EQ(heapInternalFrontWindow, heapInternalFrontWindow2);
    EXPECT_EQ(heapInternalDeviceFrontWindow, heapInternalDeviceFrontWindow2);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDeviceUsmAllocationWhenLocalOnlyFlagValueComputedThenProductHelperIsNotUsed) {
    constexpr bool preferCompressed{false};
    MockProductHelper productHelper{};
    auto hwInfo = *defaultHwInfo;
    hwInfo.caps.localOnlyAllowed = true;

    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);

    productHelper.getStorageInfoLocalOnlyFlagResult = false;
    EXPECT_EQ(memoryManager->getLocalOnlyRequired(AllocationType::buffer, productHelper, hwInfo, preferCompressed), true);
    EXPECT_EQ(memoryManager->getLocalOnlyRequired(AllocationType::svmGpu, productHelper, hwInfo, preferCompressed), true);

    EXPECT_EQ(0U, productHelper.getStorageInfoLocalOnlyFlagCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUsmDeviceAllocationModeWhenLocalOnlyFlagValueComputedForDeviceUsmAllocationThenModeIsHonored) {
    constexpr bool preferCompressed{false};
    MockProductHelper productHelper{};

    for (const bool localOnlyAllowed : {false, true}) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.caps.localOnlyAllowed = localOnlyAllowed;

        for (const auto allocationType : {AllocationType::buffer, AllocationType::svmGpu}) {
            memoryManager->usmDeviceAllocationMode = LocalMemAllocationMode::hwDefault;
            EXPECT_EQ(localOnlyAllowed, memoryManager->getLocalOnlyRequired(allocationType, productHelper, hwInfo, preferCompressed));

            memoryManager->usmDeviceAllocationMode = LocalMemAllocationMode::localOnly;
            EXPECT_TRUE(memoryManager->getLocalOnlyRequired(allocationType, productHelper, hwInfo, preferCompressed));

            memoryManager->usmDeviceAllocationMode = LocalMemAllocationMode::localPreferred;
            EXPECT_FALSE(memoryManager->getLocalOnlyRequired(allocationType, productHelper, hwInfo, preferCompressed));
        }
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenLocalOnlyModeWhenLocalOnlyFlagValueComputedForNonUsmAllocationThenModeIsIgnored) {
    MockProductHelper productHelper{};
    auto hwInfo = *defaultHwInfo;
    hwInfo.caps.localOnlyAllowed = true;
    memoryManager->usmDeviceAllocationMode = LocalMemAllocationMode::localOnly;

    EXPECT_FALSE(memoryManager->getLocalOnlyRequired(AllocationType::commandBuffer, productHelper, hwInfo, false));
    EXPECT_TRUE(memoryManager->getLocalOnlyRequired(AllocationType::commandBuffer, productHelper, hwInfo, true));
}

// Test: unbind a device virtual memory allocation fails.
HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmDeviceVMAllocationBufferObjectUnbindFails) {
    const uint64_t gpuAddr = 0x1234;
    const uint32_t rootDeviceIdx = 0u;

    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIdx]);
    MockBufferObject *bo = new MockBufferObject(rootDeviceIdx, &drm, 3, 0, 0, 1);

    MockDrmAllocation *allocation = new MockDrmAllocation(rootDeviceIdx, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation->setSize(MemoryConstants::pageSize);

    bo->setAddress(gpuAddr);
    allocation->bufferObjects[0] = bo;

    auto osContext = device->getDefaultEngine().osContext;
    auto gfxAlloc = static_cast<GraphicsAllocation *>(allocation);

    MultiGraphicsAllocation gfxAllocs{1u};
    gfxAllocs.addAllocation(gfxAlloc);

    bo->unbindErrno = EINVAL;
    EXPECT_FALSE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(gfxAllocs, gfxAlloc, gpuAddr, MemoryConstants::pageSize, osContext, rootDeviceIdx));
    errno = 0;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUnmapPhysicalDeviceMemoryWhenAllocationNotRegisteredThenReturnFalse) {
    const uint64_t gpuAddr = 0x1234;
    const uint32_t rootDeviceIdx = 0u;

    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIdx]);
    MockBufferObject *bo = new MockBufferObject(rootDeviceIdx, &drm, 3, 0, 0, 1);

    MockDrmAllocation *allocation = new MockDrmAllocation(rootDeviceIdx, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation->setSize(MemoryConstants::pageSize);

    bo->setAddress(gpuAddr);
    allocation->bufferObjects[0] = bo;

    auto osContext = device->getDefaultEngine().osContext;
    auto gfxAlloc = static_cast<GraphicsAllocation *>(allocation);

    MultiGraphicsAllocation gfxAllocs{1u};

    EXPECT_FALSE(memoryManager->unMapPhysicalDeviceMemoryFromVirtualMemory(gfxAllocs, gfxAlloc, gpuAddr, MemoryConstants::pageSize, osContext, rootDeviceIdx));

    memoryManager->freeGraphicsMemory(allocation);
}

// Test: unmap host virtual memory allocations fails.
HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmHostVMAllocationPlaceholderRestoreFails) {
    const uint64_t gpuAddr = 0x1234;
    const uint32_t rootDeviceIdx = 0u;

    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIdx]);
    MockBufferObject *bo = new MockBufferObject(rootDeviceIdx, &drm, 3, 0, 0, 1);

    MockDrmAllocation *allocation = new MockDrmAllocation(rootDeviceIdx, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
    allocation->setSize(MemoryConstants::pageSize);

    bo->setAddress(gpuAddr);
    allocation->bufferObjects[0] = bo;

    auto gfxAlloc = static_cast<GraphicsAllocation *>(allocation);

    MultiGraphicsAllocation gfxAllocs{1u};
    gfxAllocs.addAllocation(gfxAlloc);

    auto originalMmap = memoryManager->mmapFunction;
    memoryManager->mmapFunction = [](void *, size_t, int, int, int, off_t) throw()->void * { return MAP_FAILED; };
    EXPECT_FALSE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(gfxAllocs, gfxAlloc, gpuAddr, MemoryConstants::pageSize, true));
    memoryManager->mmapFunction = originalMmap;
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCpuAddressReservationIsAttemptedwithMmapFailureThenNullptrAllocationReturned) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    VariableBackup<bool> backup(&SysCalls::failMmap, true);
    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        errno = EINVAL; // Simulate mmap failure
        return MAP_FAILED;
    };
    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->mmapFunction = SysCalls::mmap; // Restore original mmap function
}
TEST_F(DrmMemoryManagerTest, givenAlignmentGreaterThan2MBWhenAllocatingGraphicsMemoryWithAlignmentImplThenProperAlignmentIsUsed) {

    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto rootDeviceEnvironment = mockExecutionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(*mockExecutionEnvironment);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    rootDeviceEnvironment->productHelper.reset(mockProductHelper.release());
    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::megaByte;
    allocationData.alignment = 4 * MemoryConstants::megaByte; // < 2MB
    allocationData.type = AllocationType::svmCpu;
    allocationData.rootDeviceIndex = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignmentImpl(allocationData);

    EXPECT_EQ(4 * MemoryConstants::megaByte, memoryManager->passedAlignment);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenMmapAlignmentLessOrEqual2MBWhenAllocatingGraphicsMemoryWithAlignmentImplThenMmapAlignmentIs2MB) {
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto rootDeviceEnvironment = mockExecutionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(*mockExecutionEnvironment);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    rootDeviceEnvironment->productHelper.reset(mockProductHelper.release());
    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::svmCpu;
    allocationData.makeGPUVaDifferentThanCPUPtr = true;
    allocationData.rootDeviceIndex = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignmentImpl(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(2 * MemoryConstants::megaByte, memoryManager->passedAlignment);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenLargeAllocationWhen2MBLocalMemAlignmentDisabledThenMmapAlignmentIs2MB) {
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto rootDeviceEnvironment = mockExecutionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(*mockExecutionEnvironment);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;
    rootDeviceEnvironment->productHelper.reset(mockProductHelper.release());
    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::buffer;
    allocationData.rootDeviceIndex = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignmentImpl(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(2 * MemoryConstants::megaByte, memoryManager->passedAlignment);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenSmallAllocationWhenAllocatingGraphicsMemoryWithAlignmentImplThenMmapAlignmentIsNotChanged) {
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto rootDeviceEnvironment = mockExecutionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(*mockExecutionEnvironment);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;
    rootDeviceEnvironment->productHelper.reset(mockProductHelper.release());
    AllocationData allocationData;
    allocationData.size = 1 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::buffer;
    allocationData.rootDeviceIndex = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignmentImpl(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryConstants::pageSize64k, memoryManager->passedAlignment);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenExactly2MBAllocationWhenAllocatingGraphicsMemoryWithAlignmentImplThenMmapAlignmentIs2MB) {
    auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto rootDeviceEnvironment = mockExecutionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(*mockExecutionEnvironment);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;
    rootDeviceEnvironment->productHelper.reset(mockProductHelper.release());
    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::buffer;
    allocationData.rootDeviceIndex = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignmentImpl(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(2 * MemoryConstants::megaByte, memoryManager->passedAlignment);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given2MBAlignmentRequiredWhenUnalignedSizePassedToAllocateWithAlignmentThenSizeAlignedTo2MB) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::megaByte + 1 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::buffer;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;

    mockIoctlHelper->is2MBSizeAlignmentRequiredResult = true;
    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignmentImpl(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(2 * MemoryConstants::megaByte, memoryManager->passedAlignment);
    EXPECT_EQ(6 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given2MBAlignmentRequiredWhenUnalignedSizePassedToAllocateByKmdThenSizeAlignedTo2MB) {
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::megaByte + 1 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::buffer;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;

    mockIoctlHelper->is2MBSizeAlignmentRequiredResult = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(6 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given2MBAllocationWhenAllocateByKMDThenAssignGpuVAFrom2MBHeap) {
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper.reset(mockIoctlHelper);

    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::megaByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::buffer;
    allocationData.rootDeviceIndex = mockRootDeviceIndex;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(HeapIndex::heapStandard2MB, memoryManager->acquireGpuRangeLastHeapIndex);
    memoryManager->freeGraphicsMemory(allocation);
}

struct MockDrmFabric : public DrmFabric {
    MockDrmFabric() = default;

    bool isAvailable() const override { return true; }

    int exportHandle(int32_t dmabufFd, uint32_t boHandle, void *reservedHandleData) override {
        exportHandleCalled++;
        lastDmabufFd = dmabufFd;
        lastBoHandle = boHandle;
        return exportHandleReturnValue;
    }

    int handleClose(const void *reservedHandleData) override {
        handleCloseCalled++;
        return -1;
    }

    int handleToFd(const void *reservedHandleData, int32_t &dmabufFd) override {
        handleToFdCalled++;
        if (handleToFdReturnValue == 0) {
            dmabufFd = 1;
        }
        return handleToFdReturnValue;
    }

    uint32_t exportHandleCalled = 0;
    int32_t lastDmabufFd = -1;
    uint32_t lastBoHandle = 0;
    int exportHandleReturnValue = -1;
    uint32_t handleCloseCalled = 0;
    uint32_t handleToFdCalled = 0;
    int handleToFdReturnValue = -1;
};

struct DrmMemoryManagerHandleTracking : public TestedDrmMemoryManager {
    using TestedDrmMemoryManager::TestedDrmMemoryManager;

    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override {
        closeInternalHandleCalled++;
        TestedDrmMemoryManager::closeInternalHandle(handle, handleId, graphicsAllocation);
    }

    uint32_t closeInternalHandleCalled = 0;
};

TEST(DrmMemoryManagerReservedHandleDataTest, givenFabricNotAvailableWhenObtainReservedHandleDataThenReturnsError) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    drm->drmFabric.reset();
    drm->setupDrmFabric();

    TestedDrmMemoryManager memoryManager(executionEnvironment);
    uint8_t reservedData[32] = {0};

    int result = memoryManager.obtainReservedHandleData(123, 0, 0, reservedData);

    EXPECT_NE(0, result);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenAvailableFabricWhenObtainReservedHandleDataThenFdAndBoHandleArePassedToExportHandle) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    auto mockFabric = new MockDrmFabric();
    mockFabric->exportHandleReturnValue = 0;
    drm->drmFabric.reset(mockFabric);

    TestedDrmMemoryManager memoryManager(executionEnvironment);
    uint8_t reservedData[32] = {0};

    int result = memoryManager.obtainReservedHandleData(123, 456, 0, reservedData);

    EXPECT_EQ(0, result);
    EXPECT_EQ(1u, mockFabric->exportHandleCalled);
    EXPECT_EQ(123, mockFabric->lastDmabufFd);
    EXPECT_EQ(456u, mockFabric->lastBoHandle);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenNullReservedHandleDataWhenCloseInternalHandleWithReservedDataThenClosesInternalHandleButNotFabricHandle) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    auto mockFabric = new MockDrmFabric();
    drm->drmFabric.reset(mockFabric);

    DrmMemoryManagerHandleTracking memoryManager(executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, AllocationType::buffer, 1});

    uint64_t handle = 123;

    memoryManager.closeInternalHandleWithReservedData(handle, 0, allocation, nullptr);

    EXPECT_NE(0u, memoryManager.closeInternalHandleCalled);
    EXPECT_EQ(0u, mockFabric->handleCloseCalled);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenNullGraphicsAllocationWhenCloseInternalHandleWithReservedDataThenReturnsEarlyWithoutClosingHandles) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    auto mockFabric = new MockDrmFabric();
    drm->drmFabric.reset(mockFabric);

    DrmMemoryManagerHandleTracking memoryManager(executionEnvironment);
    uint8_t reservedData[32] = {1, 2, 3};
    uint64_t handle = 123;

    memoryManager.closeInternalHandleWithReservedData(handle, 0, nullptr, reservedData);

    EXPECT_NE(0u, memoryManager.closeInternalHandleCalled);
    EXPECT_EQ(0u, mockFabric->handleCloseCalled);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenValidPointersWhenCloseInternalHandleWithReservedDataThenClosesBothInternalAndFabricHandles) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    auto mockFabric = new MockDrmFabric();
    drm->drmFabric.reset(mockFabric);

    DrmMemoryManagerHandleTracking memoryManager(executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, AllocationType::buffer, 1});

    uint8_t reservedData[32] = {1, 2, 3};
    uint64_t handle = 123;

    memoryManager.closeInternalHandleWithReservedData(handle, 0, allocation, reservedData);

    EXPECT_NE(0u, memoryManager.closeInternalHandleCalled);
    EXPECT_NE(0u, mockFabric->handleCloseCalled);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenFabricNotAvailableAndNullReservedHandleDataWhenGetImportHandleFromReservedHandleDataThenReturnsMinusOne) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    drm->drmFabric.reset();
    drm->setupDrmFabric();

    TestedDrmMemoryManager memoryManager(executionEnvironment);

    int result = memoryManager.getImportHandleFromReservedHandleData(nullptr, 0);

    EXPECT_EQ(-1, result);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenDrmFabricStubWhenGetImportHandleFromReservedHandleDataThenReturnsMinusOne) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    drm->drmFabric.reset();
    drm->setupDrmFabric();

    TestedDrmMemoryManager memoryManager(executionEnvironment);
    uint8_t reservedData[32] = {1, 2, 3};

    int result = memoryManager.getImportHandleFromReservedHandleData(reservedData, 0);

    EXPECT_EQ(-1, result);
}

TEST(DrmMemoryManagerReservedHandleDataTest, givenValidReservedHandleDataAndSuccessfulHandleToFdWhenGetImportHandleFromReservedHandleDataThenReturnsSuccess) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto mockIoctlHelper = new MockIoctlHelper(*drm);
    drm->ioctlHelper.reset(mockIoctlHelper);
    auto mockFabric = new MockDrmFabric();
    drm->drmFabric.reset(mockFabric);

    TestedDrmMemoryManager memoryManager(executionEnvironment);
    uint8_t reservedData[32] = {1, 2, 3};

    mockFabric->handleToFdReturnValue = 0;

    int result = memoryManager.getImportHandleFromReservedHandleData(reservedData, 0);

    EXPECT_GT(result, 0);
    EXPECT_NE(0u, mockFabric->handleToFdCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenValidMemoryInfoWhenGetCurrentUsedLocalMemorySizeThenReturnsCorrectUsedMemory) {
    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    uint32_t deviceBitfield = 1;

    uint64_t usedMemory = memoryManager->getCurrentUsedLocalMemorySize(mockRootDeviceIndex, deviceBitfield);

    uint64_t expectedUsed = probedSizeRegionOne - unallocatedSizeRegionOne;
    EXPECT_EQ(expectedUsed, usedMemory);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMultipleTilesWhenQueryingSpecificTileThenReturnsCorrectUsage) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    uint32_t regionOneBitfield = (1 << 0);
    uint64_t regionOneUsed = memoryManager->getCurrentUsedLocalMemorySize(mockRootDeviceIndex, regionOneBitfield);
    EXPECT_EQ(probedSizeRegionOne - unallocatedSizeRegionOne, regionOneUsed);

    uint32_t regionFourBitfield = (1 << 1);
    uint64_t regionFourUsed = memoryManager->getCurrentUsedLocalMemorySize(mockRootDeviceIndex, regionFourBitfield);
    EXPECT_EQ(probedSizeRegionFour - unallocatedSizeRegionFour, regionFourUsed);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenMultiTileRootDeviceBitfieldWhenGetCurrentUsedLocalMemorySizeThenReturnsSumOfAllTiles) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    uint32_t bothTilesBitfield = 0b11;
    uint64_t totalUsed = memoryManager->getCurrentUsedLocalMemorySize(mockRootDeviceIndex, bothTilesBitfield);

    uint64_t expectedUsed = (probedSizeRegionOne - unallocatedSizeRegionOne) +
                            (probedSizeRegionFour - unallocatedSizeRegionFour);
    EXPECT_EQ(expectedUsed, totalUsed);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUnifiedLocalMemoryRegionWhenGetCurrentUsedLocalMemorySizeThenSharedRegionIsCountedOnce) {
    class UnifiedRegionIoctlHelper : public MockIoctlHelper {
      public:
        using MockIoctlHelper::MockIoctlHelper;

        std::unique_ptr<MemoryInfo> createMemoryInfo() override {
            std::vector<MemoryRegion> regionInfo = {
                {
                    .region = {static_cast<uint16_t>(getDrmParamValue(DrmParam::memoryClassSystem)), 0},
                    .probedSize = probedSizeRegionZero,
                    .unallocatedSize = unallocatedSizeRegionZero,
                },
                {
                    .region = {static_cast<uint16_t>(getDrmParamValue(DrmParam::memoryClassDevice)), 0},
                    .probedSize = probedSizeRegionOne,
                    .unallocatedSize = unallocatedSizeRegionOne,
                },
            };

            return std::make_unique<MemoryInfo>(regionInfo, drm);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper = std::make_unique<UnifiedRegionIoctlHelper>(drm);

    uint32_t bothTilesBitfield = 0b11;
    uint64_t totalUsed = memoryManager->getCurrentUsedLocalMemorySize(mockRootDeviceIndex, bothTilesBitfield);

    uint64_t expectedUsed = probedSizeRegionOne - unallocatedSizeRegionOne;
    EXPECT_EQ(expectedUsed, totalUsed);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNullMemoryInfoWhenGetCurrentUsedLocalMemorySizeThenReturnsZero) {
    // Create a custom IoctlHelper that returns nullptr for createMemoryInfo
    class NullMemoryInfoIoctlHelper : public MockIoctlHelper {
      public:
        using MockIoctlHelper::MockIoctlHelper;

        std::unique_ptr<MemoryInfo> createMemoryInfo() override {
            return nullptr;
        }
    };

    auto &drm = static_cast<DrmMockCustom &>(memoryManager->getDrm(mockRootDeviceIndex));
    drm.ioctlHelper = std::make_unique<NullMemoryInfoIoctlHelper>(drm);

    uint32_t deviceBitfield = 1;
    uint64_t usedMemory = memoryManager->getCurrentUsedLocalMemorySize(mockRootDeviceIndex, deviceBitfield);

    EXPECT_EQ(0ULL, usedMemory);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenValidSystemMemoryInfoWhenGetCurrentUsedSystemSharedMemorySizeThenReturnsUsedMemory) {
    // Create a mock that simulates successful /proc/meminfo parsing
    class MockDrmMemoryManagerWithMeminfo : public TestedDrmMemoryManager {
      public:
        using TestedDrmMemoryManager::TestedDrmMemoryManager;

        bool getSystemMemoryUsageFromProcMeminfo(uint64_t &totalBytes, uint64_t &freeBytes) override {
            totalBytes = 32 * MemoryConstants::gigaByte;
            freeBytes = 16 * MemoryConstants::gigaByte;
            return true;
        }
    };

    auto memoryManager = std::make_unique<MockDrmMemoryManagerWithMeminfo>(true, false, false, *executionEnvironment);
    uint64_t usedMemory = memoryManager->getCurrentUsedSystemSharedMemorySize(mockRootDeviceIndex);

    EXPECT_EQ(16 * MemoryConstants::gigaByte, usedMemory);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenProcMeminfoReadFailsWhenGetCurrentUsedSystemSharedMemorySizeThenReturnsZero) {
    // Create a mock that simulates failed /proc/meminfo reading
    class MockDrmMemoryManagerWithMeminfoFailure : public TestedDrmMemoryManager {
      public:
        using TestedDrmMemoryManager::TestedDrmMemoryManager;

        bool getSystemMemoryUsageFromProcMeminfo(uint64_t &totalBytes, uint64_t &freeBytes) override {
            return false;
        }
    };

    auto memoryManager = std::make_unique<MockDrmMemoryManagerWithMeminfoFailure>(true, false, false, *executionEnvironment);
    uint64_t usedMemory = memoryManager->getCurrentUsedSystemSharedMemorySize(mockRootDeviceIndex);

    EXPECT_EQ(0ULL, usedMemory);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenOpenFailureWhenReadingProcMeminfoThenReturnsFalse) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *, int) {
        return -1;
    });

    const auto closeCallsBefore = SysCalls::closeFuncCalled;
    const uint64_t usedMemory = memoryManager->getCurrentUsedSystemSharedMemorySize(mockRootDeviceIndex);

    EXPECT_EQ(0ULL, usedMemory);
    EXPECT_EQ(closeCallsBefore, SysCalls::closeFuncCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenReadFailureWhenReadingProcMeminfoThenReturnsFalseAndClosesFile) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *, int) {
        return SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int, void *, size_t) {
        return static_cast<ssize_t>(-1);
    });

    const auto closeCallsBefore = SysCalls::closeFuncCalled;
    const uint64_t usedMemory = memoryManager->getCurrentUsedSystemSharedMemorySize(mockRootDeviceIndex);

    EXPECT_EQ(0ULL, usedMemory);
    EXPECT_EQ(closeCallsBefore + 1, SysCalls::closeFuncCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenValidProcMeminfoDataWhenReadingProcMeminfoThenReturnsParsedValues) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup(&SysCalls::sysCallsOpen, [](const char *, int) {
        return SysCalls::fakeFileDescriptor;
    });
    VariableBackup<decltype(SysCalls::sysCallsRead)> readBackup(&SysCalls::sysCallsRead, [](int, void *buf, size_t count) {
        static bool dataReturned = false;
        constexpr char meminfoData[] =
            "MemTotal:       1024 kB\n"
            "MemAvailable:    640 kB\n"
            "Buffers:          32 kB\n"
            "Cached:          128 kB\n"
            "SwapCached:        0 kB\n"
            "MemFree:         256 kB\n"
            "SReclaimable:     16 kB\n";

        if (dataReturned) {
            dataReturned = false;
            return static_cast<ssize_t>(0);
        }

        const size_t dataSize = sizeof(meminfoData) - 1;
        const size_t bytesToCopy = count < dataSize ? count : dataSize;
        auto destination = reinterpret_cast<char *>(buf);
        for (size_t i = 0; i < bytesToCopy; i++) {
            destination[i] = meminfoData[i];
        }
        dataReturned = true;
        return static_cast<ssize_t>(bytesToCopy);
    });

    const auto closeCallsBefore = SysCalls::closeFuncCalled;
    const uint64_t usedMemory = memoryManager->getCurrentUsedSystemSharedMemorySize(mockRootDeviceIndex);

    EXPECT_EQ((1024 - 640) * MemoryConstants::kiloByte, usedMemory);
    EXPECT_EQ(closeCallsBefore + 1, SysCalls::closeFuncCalled);
}
