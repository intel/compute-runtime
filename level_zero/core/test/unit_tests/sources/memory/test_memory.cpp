/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/memory/memory_operations_helper.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using MemoryTest = Test<DeviceFixture>;

struct CompressionMemoryTest : public MemoryTest {
    GraphicsAllocation *allocDeviceMem(size_t size) {
        ptr = nullptr;
        ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                     &deviceDesc,
                                                     size, 4096, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);

        NEO::SvmAllocationData *allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
        EXPECT_NE(nullptr, allocData);
        auto allocation = allocData->gpuAllocations.getDefaultGraphicsAllocation();
        EXPECT_NE(nullptr, allocation);

        return allocation;
    }

    DebugManagerStateRestore restore;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr = nullptr;
};

HWTEST2_F(CompressionMemoryTest, givenDeviceUsmWhenAllocatingThenEnableCompressionIfPossible, IsAtLeastSkl) {
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedBuffers = true;
    auto &hwInfo = device->getHwInfo();
    auto &l0HwHelper = L0HwHelperHw<FamilyType>::get();
    auto &hwHelper = NEO::HwHelperHw<FamilyType>::get();

    // Default path
    {
        auto allocation = allocDeviceMem(2048);

        auto supportedByDefault = l0HwHelper.usmCompressionSupported(hwInfo) && l0HwHelper.forceDefaultUsmCompressionSupport();

        EXPECT_EQ(supportedByDefault, allocation->isCompressionEnabled());

        context->freeMem(ptr);
    }

    // Compressed hint
    {
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_EQ(hwHelper.isBufferSizeSuitableForCompression(2048, hwInfo), allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Compressed hint
    {
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
        NEO::DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_TRUE(allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(-1);
        NEO::DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(-1);
    }

    // Compressed hint without debug flag
    {
        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_EQ(l0HwHelper.usmCompressionSupported(hwInfo), allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Uncompressed hint
    {
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_FALSE(allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Debug flag == 0
    {
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(0);

        auto allocation = allocDeviceMem(2048);

        EXPECT_FALSE(allocation->isCompressionEnabled());

        context->freeMem(ptr);

        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Size restriction
    {
        NEO::DebugManager.flags.RenderCompressedBuffersEnabled.set(1);

        auto allocation = allocDeviceMem(1);

        if (!hwHelper.isBufferSizeSuitableForCompression(1, hwInfo)) {
            EXPECT_FALSE(allocation->isCompressionEnabled());
        }

        context->freeMem(ptr);
    }
}

TEST_F(MemoryTest, givenDevicePointerThenDriverGetAllocPropertiesReturnsExpectedProperties) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_EQ(memoryProperties.id,
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter - 1);

    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_NE(alloc->pageSizeForAlignment, 0u);
    EXPECT_EQ(alloc->pageSizeForAlignment, memoryProperties.pageSize);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenHostPointerThenDriverGetAllocPropertiesReturnsExpectedProperties) {
    size_t size = 128;
    size_t alignment = 4096;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_EQ(memoryProperties.id,
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter - 1);

    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_NE(alloc->pageSizeForAlignment, 0u);
    EXPECT_EQ(alloc->pageSizeForAlignment, memoryProperties.pageSize);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSharedPointerThenDriverGetAllocPropertiesReturnsExpectedProperties) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_SHARED);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_EQ(memoryProperties.id,
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter - 1);

    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_NE(alloc->pageSizeForAlignment, 0u);
    EXPECT_EQ(alloc->pageSizeForAlignment, memoryProperties.pageSize);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenForceExtendedUSMBufferSizeDebugFlagWhenUSMAllocationIsCreatedThenSizeIsProperlyExtended) {
    DebugManagerStateRestore restorer;

    constexpr auto bufferSize = 16;
    auto pageSizeNumber = 2;
    NEO::DebugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
    auto extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(), &deviceDesc,
                                                 &hostDesc, bufferSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->size, extendedBufferSize);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    pageSizeNumber = 4;
    NEO::DebugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
    extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    hostDesc = {};
    result = context->allocHostMem(&hostDesc, bufferSize, alignment, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->size, extendedBufferSize);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    pageSizeNumber = 8;
    NEO::DebugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
    extendedBufferSize = bufferSize + MemoryConstants::pageSize * pageSizeNumber;

    deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, bufferSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->size, extendedBufferSize);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenHostPointerThenDriverGetAllocPropertiesReturnsMemoryId) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_EQ(deviceHandle, nullptr);
    EXPECT_EQ(memoryProperties.id,
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter - 1);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSharedPointerThenDriverGetAllocPropertiesReturnsMemoryId) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_SHARED);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_EQ(memoryProperties.id,
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter - 1);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingDeviceMemoryWithUncachedFlagThenLocallyUncachedResourceIsSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryWithUncachedFlagThenLocallyUncachedResourceIsSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryWithDeviceInitialPlacementBiasFlagThenFlagsAreSetupCorrectly) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(0u, allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementCpu);
    EXPECT_EQ(1u, allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementGpu);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryWithHostInitialPlacementBiasFlagThenFlagsAreSetupCorrectly) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT;
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(1u, allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementCpu);
    EXPECT_EQ(0u, allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementGpu);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct SVMAllocsManagerFreeExtMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerFreeExtMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager, false) {}
    bool freeSVMAlloc(void *ptr, bool blocking) override {
        if (blocking) {
            blockingCallsMade++;
        }
        return SVMAllocsManager::freeSVMAlloc(ptr, blocking);
    }
    uint32_t blockingCallsMade = 0;
};

struct FreeExtTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerFreeExtMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextImp> context;
};

TEST_F(FreeExtTests,
       whenFreeMemIsCalledWithoutArgumentThenNoBlockingCallIsMade) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    EXPECT_EQ(0u, memManager->blockingCallsMade);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledWithBlockingFreePolicyThenBlockingCallIsMade) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    EXPECT_EQ(1u, memManager->blockingCallsMade);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledWithDeferFreePolicyThenUnsuportedIsReturned) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledWithDefaultFreePolicyThenNonBlockingCallIsMade) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_free_ext_desc_t memFreeDesc = {};
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    EXPECT_EQ(0u, memManager->blockingCallsMade);
}

struct SVMAllocsManagerOutOFMemoryMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerOutOFMemoryMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager, false) {}
    void *createUnifiedMemoryAllocation(size_t size,
                                        const UnifiedMemoryProperties &svmProperties) override {
        return nullptr;
    }
};

struct OutOfMemoryTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerOutOFMemoryMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextImp> context;
};

TEST_F(OutOfMemoryTests,
       givenCallToDeviceAllocAndFailureToAllocateThenOutOfDeviceMemoryIsReturned) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize - 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
}

struct SVMAllocsManagerRelaxedSizeMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerRelaxedSizeMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager, false) {}
    void *createUnifiedMemoryAllocation(size_t size,
                                        const UnifiedMemoryProperties &svmProperties) override {
        return alignedMalloc(4096u, 4096u);
    }

    void *createSharedUnifiedMemoryAllocation(size_t size,
                                              const UnifiedMemoryProperties &svmProperties,
                                              void *cmdQ) override {
        return alignedMalloc(4096u, 4096u);
    }

    void *createHostUnifiedMemoryAllocation(size_t size,
                                            const UnifiedMemoryProperties &memoryProperties) override {
        return alignedMalloc(4096u, 4096u);
    }
};

struct ContextRelaxedSizeMock : public ContextImp {
    ContextRelaxedSizeMock(L0::DriverHandleImp *driverHandle) : ContextImp(driverHandle) {}
    ze_result_t freeMem(const void *ptr) override {
        alignedFree(const_cast<void *>(ptr));
        return ZE_RESULT_SUCCESS;
    }
};

struct MemoryRelaxedSizeTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerRelaxedSizeMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        context = std::make_unique<ContextRelaxedSizeMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextRelaxedSizeMock> context;
};

TEST_F(MemoryRelaxedSizeTests,
       givenCallToHostAllocWithAllowedSizeAndWithoutRelaxedFlagThenAllocationIsMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize - 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToHostAllocWithLargerThanAllowedSizeAndWithoutRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToHostAllocWithLargerThanAllowedSizeAndRelaxedFlagThenAllocationIsMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    hostDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToHostAllocWithLargerThanAllowedSizeAndDebugFlagThenAllocationIsMade) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowUnrestrictedSize.set(1);
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToHostAllocWithLargerThanAllowedSizeAndRelaxedFlagWithIncorrectFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = static_cast<ze_relaxed_allocation_limits_exp_flag_t>(ZE_BIT(1));
    hostDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToHostAllocWithLargerThanAllowedSizeAndRelaxedDescriptorWithWrongStypeThenUnsupportedSizeIsReturned) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
    relaxedSizeDesc.flags = static_cast<ze_relaxed_allocation_limits_exp_flag_t>(ZE_BIT(1));
    hostDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithAllowedSizeAndWithoutRelaxedFlagThenAllocationIsMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize - 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndWithoutRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndRelaxedFlagThenAllocationIsMade) {
    if (device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndDebugFlagThenAllocationIsMade) {
    if (device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowUnrestrictedSize.set(1);
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanGlobalMemSizeAndRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().globalMemSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndRelaxedFlagWithIncorrectFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = static_cast<ze_relaxed_allocation_limits_exp_flag_t>(ZE_BIT(1));
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndRelaxedDescriptorWithWrongStypeThenUnsupportedEnumerationIsReturned) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
    relaxedSizeDesc.flags = static_cast<ze_relaxed_allocation_limits_exp_flag_t>(ZE_BIT(1));
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithAllowedSizeAndWithoutRelaxedFlagThenAllocationIsMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize - 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanAllowedSizeAndWithoutRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanAllowedSizeAndRelaxedFlagThenAllocationIsMade) {
    if (device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanAllowedSizeAndDebugFlagThenAllocationIsMade) {
    if (device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowUnrestrictedSize.set(1);
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanGlobalMemSizeAndRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().globalMemSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanAllowedSizeAndRelaxedFlagWithIncorrectFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = static_cast<ze_relaxed_allocation_limits_exp_flag_t>(ZE_BIT(1));
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanAllowedSizeAndRelaxedDescriptorWithWrongStypeThenUnsupportedSizeIsReturned) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
    relaxedSizeDesc.flags = static_cast<ze_relaxed_allocation_limits_exp_flag_t>(ZE_BIT(1));
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

struct ContextMemoryTests : public MemoryRelaxedSizeTests {
    void SetUp() override {
        DebugManager.flags.AllowUnrestrictedSize.set(true);
        DebugManager.flags.CreateMultipleSubDevices.set(4);

        MemoryRelaxedSizeTests::SetUp();

        EXPECT_EQ(4u, device->getNEODevice()->getNumGenericSubDevices());
    }

    DebugManagerStateRestore restore;
};

TEST_F(ContextMemoryTests, givenMultipleSubDevicesWhenAllocatingThenUseCorrectGlobalMemorySize) {
    size_t allocationSize = neoDevice->getDeviceInfo().globalMemSize;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;

    ze_result_t result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, allocationSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocationSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);

    allocationSize /= 4;

    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, allocationSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    context->freeMem(ptr);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, allocationSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    context->freeMem(ptr);
}

struct DriverHandleFailGetFdMock : public L0::DriverHandleImp {
    void *importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAloc) override {
        importFdHandleCalledTimes++;
        if (mockFd == allocationMap.second) {
            return allocationMap.first;
        }
        return nullptr;
    }

    const int mockFd = 57;
    std::pair<void *, int> allocationMap;
    uint32_t importFdHandleCalledTimes = 0;
};

struct ContextFailFdMock : public L0::ContextImp {
    ContextFailFdMock(DriverHandleFailGetFdMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override {
        ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
        if (ZE_RESULT_SUCCESS == res) {
            driverHandle->allocationMap.first = *ptr;
            driverHandle->allocationMap.second = driverHandle->mockFd;
        }

        return res;
    }

    ze_result_t closeIpcMemHandle(const void *ptr) override {
        return ZE_RESULT_SUCCESS;
    }

    DriverHandleFailGetFdMock *driverHandle = nullptr;
};

struct MemoryExportImportFailTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleFailGetFdMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextFailFdMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
    }
    std::unique_ptr<DriverHandleFailGetFdMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextFailFdMock> context;
};

TEST_F(MemoryExportImportFailTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndIncorrectStypeThenFileDescriptorIsNotReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_EQ(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_NE(extendedProperties.fd, driverHandle->mockFd);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportFailTest,
       whenParsingMemoryTypeWithNotSpecifidTypeThenUnknownTypeIsReturned) {

    InternalMemoryType memoryType = InternalMemoryType::NOT_SPECIFIED;
    ze_memory_type_t usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_UNKNOWN);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedExportDescriptorAndNonSupportedFlagThenUnsuportedEnumerationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedExportDescriptorAndSupportedFlagThenAllocationIsMade) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndUnsupportedFlagThenUnsupportedEnumerationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    EXPECT_EQ(extendedProperties.fd, std::numeric_limits<int>::max());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesForNonDeviceAllocationThenUnsupportedFeatureIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    result = context->getMemAllocProperties(ptr, &memoryProperties, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(extendedProperties.fd, std::numeric_limits<int>::max());

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndSupportedFlagThenValidFileDescriptorIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedImportDescriptorAndNonSupportedFlagThenUnsupportedEnumerationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());
    EXPECT_NE(extendedProperties.fd, std::numeric_limits<int>::max());
    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);

    ze_device_mem_alloc_desc_t importDeviceDesc = {};
    ze_external_memory_import_fd_t extendedImportDesc = {};
    extendedImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    extendedImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    extendedImportDesc.fd = extendedProperties.fd;
    importDeviceDesc.pNext = &extendedImportDesc;

    void *importedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(),
                                     &importDeviceDesc,
                                     size, alignment, &importedPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    EXPECT_EQ(nullptr, importedPtr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportWinHandleTest,
       givenCallToDeviceAllocWithExtendedExportDescriptorAndNTHandleFlagThenAllocationIsMade) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    deviceDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportWinHandleTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndUnsupportedFlagThenUnsupportedEnumerationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_win32_handle_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP;
    extendedProperties.handle = nullptr;
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    EXPECT_EQ(extendedProperties.handle, nullptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportWinHandleTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndSupportedFlagThenValidHandleIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_external_memory_export_win32_handle_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    memoryProperties.pNext = &extendedProperties;

    ze_device_handle_t deviceHandle;
    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(extendedProperties.handle, reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(driverHandle->mockHandle)));

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(MemoryTest,
          givenCallToGetImageAllocPropertiesWithNoBackingAllocationErrorIsReturned, IsAtLeastSkl) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    // uninitialized, so no backing graphics allocation
    struct ImageCoreFamily<gfxCoreFamily> image = {};

    ze_result_t result = context->getImageAllocProperties(&image, &imageProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
}

struct ImageWindowsExportImportTest : public MemoryExportImportWinHandleTest {
    void SetUp() override {
        MemoryExportImportWinHandleTest::SetUp();

        ze_image_desc_t zeDesc = {};
        zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        zeDesc.arraylevels = 1u;
        zeDesc.depth = 1u;
        zeDesc.height = 1u;
        zeDesc.width = 1u;
        zeDesc.miplevels = 1u;
        zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
        zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

        zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                         ZE_IMAGE_FORMAT_TYPE_UINT,
                         ZE_IMAGE_FORMAT_SWIZZLE_R,
                         ZE_IMAGE_FORMAT_SWIZZLE_G,
                         ZE_IMAGE_FORMAT_SWIZZLE_B,
                         ZE_IMAGE_FORMAT_SWIZZLE_A};

        auto result = Image::create(productFamily, device, &zeDesc, &image);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    }

    void TearDown() override {
        image->destroy();
        MemoryExportImportWinHandleTest::TearDown();
    }

    L0::Image *image;
};

TEST_F(ImageWindowsExportImportTest,
       givenCallToGetImageAllocPropertiesThenIdIsReturned) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    ze_result_t result = context->getImageAllocProperties(image, &imageProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(imageProperties.id, 0ul);
}

TEST_F(ImageWindowsExportImportTest,
       givenCallToGetImageAllocPropertiesWithExtendedExportPropertiesThenValidHandleIsReturned) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    ze_external_memory_export_win32_handle_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    extendedProperties.handle = nullptr;
    imageProperties.pNext = &extendedProperties;

    ze_result_t result = context->getImageAllocProperties(image, &imageProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(imageProperties.id, 0ul);

    EXPECT_NE(extendedProperties.handle, nullptr);
    EXPECT_EQ(extendedProperties.handle, reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(driverHandle->mockHandle)));
}

TEST_F(ImageWindowsExportImportTest,
       givenCallToGetImageAllocPropertiesWithExtendedExportPropertiesAndInvalidStructureTypeThenErrorIsReturned) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    ze_external_memory_export_win32_handle_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    extendedProperties.handle = nullptr;
    imageProperties.pNext = &extendedProperties;

    ze_result_t result = context->getImageAllocProperties(image, &imageProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, result);
}

TEST_F(ImageWindowsExportImportTest,
       givenCallToGetImageAllocPropertiesWithExtendedExportPropertiesAndUnsupportedFlagsThenErrorIsReturned) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    ze_external_memory_export_win32_handle_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE;
    extendedProperties.handle = nullptr;
    imageProperties.pNext = &extendedProperties;

    ze_result_t result = context->getImageAllocProperties(image, &imageProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
}

struct ImageFdExportImportTest : public MemoryExportImportTest {
    void SetUp() override {
        MemoryExportImportTest::SetUp();

        ze_image_desc_t zeDesc = {};
        zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        zeDesc.arraylevels = 1u;
        zeDesc.depth = 1u;
        zeDesc.height = 1u;
        zeDesc.width = 1u;
        zeDesc.miplevels = 1u;
        zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;
        zeDesc.flags = ZE_IMAGE_FLAG_BIAS_UNCACHED;

        zeDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32,
                         ZE_IMAGE_FORMAT_TYPE_UINT,
                         ZE_IMAGE_FORMAT_SWIZZLE_R,
                         ZE_IMAGE_FORMAT_SWIZZLE_G,
                         ZE_IMAGE_FORMAT_SWIZZLE_B,
                         ZE_IMAGE_FORMAT_SWIZZLE_A};

        auto result = Image::create(productFamily, device, &zeDesc, &image);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    }

    void TearDown() override {
        image->destroy();
        MemoryExportImportTest::TearDown();
    }

    L0::Image *image;
};

TEST_F(ImageFdExportImportTest,
       givenCallToGetImageAllocPropertiesWithExtendedExportPropertiesThenValidFileDescriptorIsReturned) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    extendedProperties.fd = std::numeric_limits<int>::max();
    imageProperties.pNext = &extendedProperties;

    ze_result_t result = context->getImageAllocProperties(image, &imageProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(imageProperties.id, 0ul);

    EXPECT_EQ(extendedProperties.fd, driverHandle->mockFd);
}

struct MultipleDevicePeerAllocationFailTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, 0, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<DriverHandleFailGetFdMock>();
        driverHandle->initialize(std::move(devices));
        driverHandle->setMemoryManager(driverHandle->getMemoryManager());

        context = std::make_unique<ContextShareableMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->toHandle(), device));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<DriverHandleFailGetFdMock> driverHandle;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextShareableMock> context;

    const uint32_t numRootDevices = 2u;
};

TEST_F(MultipleDevicePeerAllocationFailTest,
       givenImportFdHandleFailedThenPeerAllocationReturnsNullptr) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);

    DriverHandleFailGetFdMock *driverHandleFailGetFdMock = static_cast<DriverHandleFailGetFdMock *>(context->getDriverHandle());
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
    EXPECT_GT(driverHandleFailGetFdMock->importFdHandleCalledTimes, 0u);
    EXPECT_EQ(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct MultipleDevicePeerAllocationTest : public ::testing::Test {
    void createModuleFromBinary(L0::Device *device, ModuleType type = ModuleType::User) {
        std::string testFile;
        retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

        size_t size = 0;
        auto src = loadDataFromFile(
            testFile.c_str(),
            size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;

        module.reset(Module::create(device, &moduleDesc, moduleBuildLog, type));
    }

    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerOpenIpcMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        context = std::make_unique<ContextShareableMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->toHandle(), device));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
    }

    void createKernel() {
        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
        kernel->module = module.get();
        kernel->initialize(&desc);
    }

    void TearDown() override {
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    DebugManagerStateRestore restorer;
    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextShareableMock> context;

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenCallToMPrepareIndirectAllocationForDestructionThenOnlyValidAllocationCountsAreUpdated,
          IsAtLeastSkl) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr0 = nullptr;
    L0::DeviceImp *deviceImp0 = static_cast<L0::DeviceImp *>(device0);
    auto ret = deviceImp0->getCsrForOrdinalAndIndex(&csr0, 0u, 0u);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);

    size_t size = 1024;
    size_t alignment = 1u;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr0 = nullptr;
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr0);
    void *ptr1 = nullptr;
    result = context->allocDeviceMem(device1->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1);

    auto allocationData1 = svmManager->getSVMAlloc(ptr1);
    uint32_t prevPeekTaskCount1 = allocationData1->gpuAllocations.getGraphicsAllocation(1u)->getTaskCount(csr0->getOsContext().getContextId());
    svmManager->prepareIndirectAllocationForDestruction(allocationData1);
    uint32_t postPeekTaskCount1 = allocationData1->gpuAllocations.getGraphicsAllocation(1u)->getTaskCount(csr0->getOsContext().getContextId());

    EXPECT_EQ(postPeekTaskCount1, prevPeekTaskCount1);

    ret = context->freeMem(ptr0);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
    ret = context->freeMem(ptr1);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          whenisRemoteResourceNeededIsCalledWithDifferentCombinationsOfInputsThenExpectedOutputIsReturned,
          IsAtLeastSkl) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr0 = nullptr;
    L0::DeviceImp *deviceImp0 = static_cast<L0::DeviceImp *>(device0);
    auto ret = deviceImp0->getCsrForOrdinalAndIndex(&csr0, 0u, 0u);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);

    size_t size = 1024;
    size_t alignment = 1u;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr0 = nullptr;
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr0);
    void *ptr1 = nullptr;
    result = context->allocDeviceMem(device1->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1);

    auto allocationData0 = svmManager->getSVMAlloc(ptr0);
    auto allocationData1 = svmManager->getSVMAlloc(ptr1);

    bool isNeeded = driverHandle->isRemoteResourceNeeded(ptr0, nullptr, allocationData1, device0);
    EXPECT_TRUE(isNeeded);

    isNeeded = driverHandle->isRemoteResourceNeeded(ptr0, allocationData0->gpuAllocations.getGraphicsAllocation(0u), allocationData0, device0);
    EXPECT_FALSE(isNeeded);

    isNeeded = driverHandle->isRemoteResourceNeeded(ptr0, allocationData0->gpuAllocations.getGraphicsAllocation(1u), nullptr, device0);
    EXPECT_TRUE(isNeeded);

    isNeeded = driverHandle->isRemoteResourceNeeded(ptr0, allocationData0->gpuAllocations.getGraphicsAllocation(0u), allocationData0, device1);
    EXPECT_TRUE(isNeeded);

    ret = context->freeMem(ptr0);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
    ret = context->freeMem(ptr1);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenCallToMakeIndirectAllocationsResidentThenOnlyValidAllocationsAreMadeResident,
          IsAtLeastSkl) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr = nullptr;
    L0::DeviceImp *deviceImp1 = static_cast<L0::DeviceImp *>(device1);
    auto ret = deviceImp1->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);

    size_t size = 1024;
    size_t alignment = 1u;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr0 = nullptr;
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr0);
    void *ptr1 = nullptr;
    result = context->allocDeviceMem(device1->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1);

    auto &residentAllocations = csr->getResidencyAllocations();
    EXPECT_EQ(0u, residentAllocations.size());
    svmManager->makeIndirectAllocationsResident(*csr, 1u);
    EXPECT_EQ(1u, residentAllocations.size());
    EXPECT_EQ(residentAllocations[0]->getGpuAddress(), reinterpret_cast<uint64_t>(ptr1));

    ret = context->freeMem(ptr0);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
    ret = context->freeMem(ptr1);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenCallToMakeInternalAllocationsResidentThenOnlyValidAllocationsAreMadeResident,
          IsAtLeastSkl) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr = nullptr;
    L0::DeviceImp *deviceImp1 = static_cast<L0::DeviceImp *>(device1);
    auto ret = deviceImp1->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);

    size_t size = 1024;
    size_t alignment = 1u;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr0 = nullptr;
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr0);
    void *ptr1 = nullptr;
    result = context->allocDeviceMem(device1->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1);

    auto &residentAllocations = csr->getResidencyAllocations();
    EXPECT_EQ(0u, residentAllocations.size());
    svmManager->makeInternalAllocationsResident(*csr, InternalMemoryType::DEVICE_UNIFIED_MEMORY);
    EXPECT_EQ(1u, residentAllocations.size());
    EXPECT_EQ(residentAllocations[0]->getGpuAddress(), reinterpret_cast<uint64_t>(ptr1));

    ret = context->freeMem(ptr0);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
    ret = context->freeMem(ptr1);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToAppendBlitFillAndImportFdHandleFailingThenInvalidArgumentIsReturned,
          IsAtLeastSkl) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::RenderCompute, 0u);

    uint32_t pattern = 1;
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToAppendBlitFillUsingSameDeviceThenSuccessIsReturned,
          IsAtLeastSkl) {
    L0::Device *device0 = driverHandle->devices[0];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::RenderCompute, 0u);

    uint32_t pattern = 1;
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToAppendBlitFillUsingDevice1ThenSuccessIsReturned,
          IsAtLeastSkl) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::RenderCompute, 0u);

    uint32_t pattern = 1;
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToAppendBlitFillUsingDevice0ThenSuccessIsReturned,
          IsAtLeastSkl) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device1->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::RenderCompute, 0u);

    uint32_t pattern = 1;
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenHostPointerAllocationPassedToAppendBlitFillUsingDevice0ThenInvalidArgumentIsReturned,
          IsAtLeastSkl) {
    L0::Device *device0 = driverHandle->devices[0];

    size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::RenderCompute, 0u);

    uint32_t pattern = 1;
    ze_result_t result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    delete[] ptr;
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToGetAllignedAllocationAndImportFdHandleFailingThenPeerAllocNotFoundReturnsTrue,
          IsAtLeastSkl) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::RenderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device1, ptr, size, false);
    EXPECT_EQ(nullptr, outData.alloc);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToGetAllignedAllocationUsingDevice1ThenAlignedAllocationWithPeerAllocationIsReturned,
          IsAtLeastSkl) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::RenderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device1, ptr, size, false);
    EXPECT_NE(outData.alignedAllocationPtr, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerAllocationTest,
          givenDeviceAllocationPassedToGetAllignedAllocationUsingDevice0ThenAlignedAllocationWithPeerAllocationIsReturned,
          IsAtLeastSkl) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device1->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::RenderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device0, ptr, size, false);
    EXPECT_NE(outData.alignedAllocationPtr, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest,
         givenDeviceAllocationPassedAsArgumentToKernelInPeerDeviceThenPeerAllocationIsUsed) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    L0::DeviceImp *deviceImp1 = static_cast<L0::DeviceImp *>(device1);

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(), &deviceDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    void *ptr1 = nullptr;
    result = context->allocDeviceMem(device0->toHandle(), &deviceDesc, size, alignment, &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1);

    createModuleFromBinary(device1);
    createKernel();

    // set argument in device 1's list with ptr from device 0: peer allocation is created
    result = kernel->setArgBuffer(0, sizeof(ptr), &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(deviceImp1->peerAllocations.getNumAllocs()), 1u);

    // set argument in device 1's list with ptr1 from device 0: anoter peer allocation is created
    result = kernel->setArgBuffer(0, sizeof(ptr), &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(deviceImp1->peerAllocations.getNumAllocs()), 2u);

    // set argument in device 1's list with ptr from device 0 plus offset: no new peer allocation is created
    // since a peer allocation is already avialable
    void *ptrOffset = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 4);
    result = kernel->setArgBuffer(0, sizeof(ptr), &ptrOffset);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(deviceImp1->peerAllocations.getNumAllocs()), 2u);

    result = context->freeMem(ptr1);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest,
         givenDeviceAllocationPassedAsArgumentToKernelInPeerDeviceAndCreationOfSharedHandleAllocationFailedThenInvalidArgumentIsReturned) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    createModuleFromBinary(device1);
    createKernel();

    result = kernel->setArgBuffer(0, sizeof(ptr), &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedAndImportFdHandleFailingThenNullptrIsReturned) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
    EXPECT_EQ(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedThenPeerAllocationIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
    EXPECT_NE(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedThenPeerAllocationIsAddedToDeviceMapAndRemovedWhenAllocationIsFreed) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
    EXPECT_NE(peerAlloc, nullptr);

    DeviceImp *deviceImp1 = static_cast<DeviceImp *>(device1);
    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_NE(iter, deviceImp1->peerAllocations.allocations.end());
    }

    result = context->freeMem(ptr);

    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_EQ(iter, deviceImp1->peerAllocations.allocations.end());
    }

    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedThenPeerAllocationIsAddedToDeviceMapAndReturnedWhenLookingForPeerAllocationAgain) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uintptr_t peerGpuAddress = 0u;
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);

    DeviceImp *deviceImp1 = static_cast<DeviceImp *>(device1);
    EXPECT_EQ(0u, deviceImp1->peerAllocations.allocations.size());
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress);
    EXPECT_NE(peerAlloc, nullptr);
    EXPECT_EQ(1u, deviceImp1->peerAllocations.allocations.size());

    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_NE(iter, deviceImp1->peerAllocations.allocations.end());
    }

    uintptr_t peerGpuAddress2 = 0u;
    peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress2);
    EXPECT_NE(peerAlloc, nullptr);
    EXPECT_EQ(1u, deviceImp1->peerAllocations.allocations.size());
    EXPECT_EQ(peerGpuAddress, peerGpuAddress2);

    result = context->freeMem(ptr);

    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_EQ(iter, deviceImp1->peerAllocations.allocations.end());
    }

    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedWithoutPassingPeerGpuAddressParameterThenPeerAllocationIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, nullptr);
    EXPECT_NE(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedTwiceThenSamePeerAllocationIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);

    uintptr_t peerGpuAddress0 = 0u;
    auto peerAlloc0 = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress0);
    EXPECT_NE(peerAlloc0, nullptr);

    uintptr_t peerGpuAddress1 = 0u;
    auto peerAlloc1 = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress1);
    EXPECT_NE(peerAlloc1, nullptr);

    EXPECT_EQ(peerAlloc0, peerAlloc1);
    EXPECT_EQ(peerGpuAddress0, peerGpuAddress1);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct MemoryFailedOpenIpcHandleTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerIpcMock(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);
        device = driverHandle->devices[0];

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }
    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextImp> context;
};

TEST_F(MemoryFailedOpenIpcHandleTest,
       givenCallToOpenIpcMemHandleWithNullPtrFromCreateGraphicsAllocationFromSharedHandleThenInvalidArgumentIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(ipcPtr, nullptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using DeviceMemorySizeTest = Test<DeviceFixture>;

TEST_F(DeviceMemorySizeTest, givenSizeGreaterThanLimitThenDeviceAllocationFails) {
    size_t size = neoDevice->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device,
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
}

TEST_F(MemoryTest, givenSharedPointerThenDriverGetAllocPropertiesReturnsDeviceHandle) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_SHARED);
    EXPECT_EQ(deviceHandle, device->toHandle());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenHostPointerThenDriverGetAllocPropertiesReturnsNullDevice) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_EQ(deviceHandle, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSystemAllocatedPointerThenDriverGetAllocPropertiesReturnsUnknownType) {
    size_t size = 10;
    int *ptr = new int[size];

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;
    ze_result_t result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_UNKNOWN);

    delete[] ptr;
}

TEST_F(MemoryTest, givenSharedPointerAndDeviceHandleAsNullThenDriverReturnsSuccessAndReturnsPointerToSharedAllocation) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ASSERT_NE(nullptr, device->toHandle());
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(nullptr,
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenNoDeviceWhenAllocatingSharedMemoryThenDeviceInAllocationIsNullptr) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ASSERT_NE(nullptr, device->toHandle());
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(nullptr,
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    auto alloc = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(alloc->device, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenCallToCheckMemoryAccessFromDeviceWithInvalidPointerThenInvalidArgumentIsReturned) {
    void *ptr = nullptr;
    ze_result_t res = driverHandle->checkMemoryAccessFromDevice(device, ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

TEST_F(MemoryTest, givenCallToCheckMemoryAccessFromDeviceWithValidDeviceAllocationPointerThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t res = context->allocDeviceMem(device->toHandle(),
                                              &deviceDesc,
                                              size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    res = driverHandle->checkMemoryAccessFromDevice(device, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MemoryTest, givenCallToCheckMemoryAccessFromDeviceWithValidSharedAllocationPointerThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t res = context->allocSharedMem(device->toHandle(),
                                              &deviceDesc,
                                              &hostDesc,
                                              size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    res = driverHandle->checkMemoryAccessFromDevice(device, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(MemoryTest, givenCallToCheckMemoryAccessFromDeviceWithValidHostAllocationPointerThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t res = context->allocHostMem(&hostDesc,
                                            size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    res = driverHandle->checkMemoryAccessFromDevice(device, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

struct MemoryBitfieldTest : testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        memoryManager = new NEO::MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        neoDevice = NEO::Device::create<RootDevice>(executionEnvironment, 0u);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
        memoryManager->recentlyPassedDeviceBitfield = {};

        ASSERT_NE(nullptr, driverHandle->devices[0]->toHandle());
        EXPECT_NE(neoDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        auto result = context->freeMem(ptr);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    NEO::MockMemoryManager *memoryManager;
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
    std::unique_ptr<ContextImp> context;
    NEO::ExecutionEnvironment *executionEnvironment;
};

TEST_F(MemoryBitfieldTest, givenDeviceWithValidBitfieldWhenAllocatingDeviceMemoryThenPassProperBitfield) {
    NEO::MockCompilerEnableGuard mock(true);
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &ptr);
    EXPECT_EQ(neoDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
}
TEST(MemoryBitfieldTests, givenDeviceWithValidBitfieldWhenAllocatingSharedMemoryThenPassProperBitfield) {
    NEO::MockCompilerEnableGuard mock(true);
    DebugManagerStateRestore restorer;
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (size_t i = 0; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
    }
    auto memoryManager = new NEO::MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    NEO::Device *neoDevice0 = NEO::Device::create<RootDevice>(executionEnvironment, 0u);
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    NEO::Device *neoDevice1 = NEO::Device::create<RootDevice>(executionEnvironment, 1u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice0));
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice1));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto device = driverHandle->devices[0];
    std::unique_ptr<ContextImp> context;
    context = std::make_unique<ContextImp>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->toHandle(), device));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});

    memoryManager->recentlyPassedDeviceBitfield = {};
    ASSERT_NE(nullptr, driverHandle->devices[1]->toHandle());
    EXPECT_NE(neoDevice0->getDeviceBitfield(), neoDevice1->getDeviceBitfield());
    EXPECT_NE(neoDevice0->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(nullptr,
                                          &deviceDesc,
                                          &hostDesc,
                                          size, alignment, &ptr);
    EXPECT_EQ(neoDevice0->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    memoryManager->recentlyPassedDeviceBitfield = {};
    EXPECT_NE(neoDevice1->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    result = context->allocSharedMem(driverHandle->devices[0]->toHandle(),
                                     &deviceDesc,
                                     &hostDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(neoDevice0->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct AllocHostMemoryTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(),
                                                                                                                                executionEnvironment, i)));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));
    }

    void TearDown() override {
        context->destroy();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    const uint32_t numRootDevices = 2u;
    L0::ContextImp *context = nullptr;
};

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemThenAllocateGraphicsMemoryWithPropertiesIsCalledTheNumberOfTimesOfRootDevices) {
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->isMockHostMemoryManager = true;
    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               4096u, 0u, &ptr);
    EXPECT_EQ(static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount, numRootDevices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    context->freeMem(ptr);
}

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemAndFailingOnCreatingGraphicsAllocationThenNullIsReturned) {

    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->isMockHostMemoryManager = true;
    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->forceFailureInPrimaryAllocation = true;

    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               4096u, 0u, &ptr);
    EXPECT_EQ(static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount, 1u);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemAndFailingOnCreatingGraphicsAllocationWithHostPointerThenNullIsReturned) {

    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->isMockHostMemoryManager = true;
    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;

    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               4096u, 0u, &ptr);
    EXPECT_EQ(static_cast<MockMemoryManager *>(driverHandle.get()->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount, numRootDevices);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(nullptr, ptr);
}

using ContextMemoryTest = Test<ContextFixture>;

TEST_F(ContextMemoryTest, whenAllocatingSharedAllocationFromContextThenAllocationSucceeds) {
    size_t size = 10u;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenAllocatingHostAllocationFromContextThenAllocationSucceeds) {
    size_t size = 10u;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenAllocatingDeviceAllocationFromContextThenAllocationSucceeds) {
    size_t size = 10u;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenRetrievingAddressRangeForDeviceAllocationThenRangeIsCorrect) {
    size_t allocSize = 4096u;
    size_t alignment = 1u;
    void *allocPtr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 allocSize, alignment, &allocPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocPtr);

    void *base = nullptr;
    size_t size = 0u;
    void *pPtr = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(allocPtr) + 77);
    result = context->getMemAddressRange(pPtr, &base, &size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(base, allocPtr);
    EXPECT_GE(size, allocSize);

    result = context->freeMem(allocPtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenRetrievingSizeForDeviceAllocationThenUserSizeIsReturned) {
    size_t allocSize = 100;
    size_t alignment = 1u;
    void *allocPtr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 allocSize, alignment, &allocPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocPtr);

    void *base = nullptr;
    size_t size = 0u;
    void *pPtr = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(allocPtr) + 77);
    result = context->getMemAddressRange(pPtr, &base, &size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(base, allocPtr);
    EXPECT_EQ(size, allocSize);

    result = context->freeMem(allocPtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenRetrievingAddressRangeForDeviceAllocationWithNoBaseArgumentThenSizeIsCorrectAndSuccessIsReturned) {
    size_t allocSize = 4096u;
    size_t alignment = 1u;
    void *allocPtr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 allocSize, alignment, &allocPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocPtr);

    size_t size = 0u;
    void *pPtr = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(allocPtr) + 77);
    result = context->getMemAddressRange(pPtr, nullptr, &size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GE(size, allocSize);

    result = context->freeMem(allocPtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenRetrievingAddressRangeForDeviceAllocationWithNoSizeArgumentThenRangeIsCorrectAndSuccessIsReturned) {
    size_t allocSize = 4096u;
    size_t alignment = 1u;
    void *allocPtr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 allocSize, alignment, &allocPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocPtr);

    void *base = nullptr;
    void *pPtr = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(allocPtr) + 77);
    result = context->getMemAddressRange(pPtr, &base, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(base, allocPtr);

    result = context->freeMem(allocPtr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(ContextMemoryTest, whenRetrievingAddressRangeForUnknownDeviceAllocationThenResultUnknownIsReturned) {
    void *base = nullptr;
    size_t size = 0u;
    uint64_t var = 0;
    ze_result_t res = context->getMemAddressRange(&var, &base, &size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
}

TEST_F(ContextMemoryTest, givenSystemAllocatedPointerThenGetAllocPropertiesReturnsUnknownType) {
    size_t size = 10;
    int *ptr = new int[size];

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;
    ze_result_t result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_UNKNOWN);

    delete[] ptr;
}

TEST_F(ContextMemoryTest, givenCallTochangeMemoryOperationStatusToL0ResultTypeThenExpectedValueIsReturned) {
    NEO::MemoryOperationsStatus status = NEO::MemoryOperationsStatus::SUCCESS;
    ze_result_t res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    status = NEO::MemoryOperationsStatus::FAILED;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_DEVICE_LOST);

    status = NEO::MemoryOperationsStatus::MEMORY_NOT_FOUND;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    status = NEO::MemoryOperationsStatus::OUT_OF_MEMORY;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);

    status = NEO::MemoryOperationsStatus::UNSUPPORTED;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    status = NEO::MemoryOperationsStatus::DEVICE_UNINITIALIZED;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNINITIALIZED);

    status = static_cast<NEO::MemoryOperationsStatus>(static_cast<uint32_t>(NEO::MemoryOperationsStatus::DEVICE_UNINITIALIZED) + 1);
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNKNOWN);
}

using ImportFdUncachedTests = MemoryOpenIpcHandleTest;

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithUncachedFlagsThenLocallyUncachedResourceIsSet) {
    ze_ipc_memory_flags_t flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    uint64_t handle = 1;
    void *ptr = driverHandle->importFdHandle(device->toHandle(), flags, handle, nullptr);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithUncachedIpcFlagsThenLocallyUncachedResourceIsSet) {
    ze_ipc_memory_flags_t flags = ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED;
    uint64_t handle = 1;
    void *ptr = driverHandle->importFdHandle(device->toHandle(), flags, handle, nullptr);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithBothUncachedFlagsThenLocallyUncachedResourceIsSet) {
    ze_ipc_memory_flags_t flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED | ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED;
    uint64_t handle = 1;
    void *ptr = driverHandle->importFdHandle(device->toHandle(), flags, handle, nullptr);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithoutUncachedFlagsThenLocallyUncachedResourceIsNotSet) {
    ze_ipc_memory_flags_t flags = {};
    uint64_t handle = 1;
    void *ptr = driverHandle->importFdHandle(device->toHandle(), flags, handle, nullptr);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);

    context->freeMem(ptr);
}

struct SVMAllocsManagerSharedAllocFailMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerSharedAllocFailMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager, false) {}
    void *createSharedUnifiedMemoryAllocation(size_t size,
                                              const UnifiedMemoryProperties &svmProperties,
                                              void *cmdQ) override {
        return nullptr;
    }
};

struct SharedAllocFailTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerSharedAllocFailMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        context = std::make_unique<ContextImp>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextImp> context;
};

TEST_F(SharedAllocFailTests, whenAllocatinSharedMemoryAndAllocationFailsThenOutOfDeviceMemoryIsReturned) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *ptr = nullptr;
    size_t size = 1024;
    ze_result_t res = context->allocSharedMem(nullptr, &deviceDesc, &hostDesc, size, 0u, &ptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

struct SVMAllocsManagerSharedAllocMultiDeviceMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerSharedAllocMultiDeviceMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager, false) {}
    void *createHostUnifiedMemoryAllocation(size_t size,
                                            const UnifiedMemoryProperties &memoryProperties) override {
        createHostUnifiedMemoryAllocationTimes++;
        return alignedMalloc(4096u, 4096u);
    }

    uint32_t createHostUnifiedMemoryAllocationTimes = 0;
};

struct ContextMultiDeviceMock : public L0::ContextImp {
    ContextMultiDeviceMock(L0::DriverHandleImp *driverHandle) : L0::ContextImp(driverHandle) {}
    ze_result_t freeMem(const void *ptr) override {
        SVMAllocsManagerSharedAllocMultiDeviceMock *currSvmAllocsManager =
            static_cast<SVMAllocsManagerSharedAllocMultiDeviceMock *>(this->driverHandle->svmAllocsManager);
        if (currSvmAllocsManager->createHostUnifiedMemoryAllocationTimes == 0) {
            return ContextImp::freeMem(ptr);
        }
        alignedFree(const_cast<void *>(ptr));
        return ZE_RESULT_SUCCESS;
    }
    bool isShareableMemory(const void *pNext, bool exportableMemory, NEO::Device *neoDevice) override {
        return true;
    }
};

struct SharedAllocMultiDeviceTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        auto executionEnvironment = new NEO::ExecutionEnvironment;
        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        driverHandle = std::make_unique<DriverHandleImp>();
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerSharedAllocMultiDeviceMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context = std::make_unique<ContextMultiDeviceMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);

        for (uint32_t i = 0; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->toHandle(), device));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }

    DebugManagerStateRestore restorer;
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    SVMAllocsManagerSharedAllocMultiDeviceMock *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    std::unique_ptr<ContextMultiDeviceMock> context;
    const uint32_t numRootDevices = 4u;
};

TEST_F(SharedAllocMultiDeviceTests, whenAllocatinSharedMemoryWithNullDeviceInAMultiDeviceSystemThenHostAllocationIsCreated) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *ptr = nullptr;
    size_t size = 1024;
    EXPECT_EQ(currSvmAllocsManager->createHostUnifiedMemoryAllocationTimes, 0u);
    ze_result_t res = context->allocSharedMem(nullptr, &deviceDesc, &hostDesc, size, 0u, &ptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(currSvmAllocsManager->createHostUnifiedMemoryAllocationTimes, 1u);

    res = context->freeMem(ptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(SharedAllocMultiDeviceTests, whenAllocatinSharedMemoryWithNonNullDeviceInAMultiDeviceSystemThenDeviceAllocationIsCreated) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *ptr = nullptr;
    size_t size = 1024;
    ze_result_t res = ZE_RESULT_ERROR_UNKNOWN;
    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;
    EXPECT_EQ(currSvmAllocsManager->createHostUnifiedMemoryAllocationTimes, 0u);
    for (uint32_t i = 0; i < numRootDevices; i++) {
        res = context->allocSharedMem(driverHandle->devices[i]->toHandle(), &deviceDesc, &hostDesc, size, 0u, &ptr);
        EXPECT_EQ(res, ZE_RESULT_SUCCESS);
        res = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_SHARED);
        EXPECT_EQ(deviceHandle, driverHandle->devices[i]->toHandle());
        res = context->freeMem(ptr);
        EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    }
    EXPECT_EQ(currSvmAllocsManager->createHostUnifiedMemoryAllocationTimes, 0u);
}

template <int32_t enableWalkerPartition>
struct MemAllocMultiSubDeviceTests : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.EnableWalkerPartition.set(enableWalkerPartition);
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        auto executionEnvironment = new NEO::ExecutionEnvironment;
        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        driverHandle = std::make_unique<DriverHandleImp>();
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerSharedAllocMultiDeviceMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context = std::make_unique<ContextMultiDeviceMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);

        for (uint32_t i = 0; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->toHandle(), device));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }

    DebugManagerStateRestore restorer;
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    SVMAllocsManagerSharedAllocMultiDeviceMock *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    std::unique_ptr<ContextMultiDeviceMock> context;
    const uint32_t numSubDevices = 2u;
    const uint32_t numRootDevices = 1u;
};

using MemAllocMultiSubDeviceTestsDisabledImplicitScaling = MemAllocMultiSubDeviceTests<0>;
using MemAllocMultiSubDeviceTestsEnabledImplicitScaling = MemAllocMultiSubDeviceTests<1>;

TEST_F(MemAllocMultiSubDeviceTestsDisabledImplicitScaling, GivenImplicitScalingDisabledWhenAllocatingDeviceMemorySubDeviceMemorySizeUsedThenExpectCorrectErrorReturned) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr = nullptr;
    size_t size = driverHandle->devices[0]->getNEODevice()->getDeviceInfo().globalMemSize;
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;

    ze_result_t res = context->allocDeviceMem(driverHandle->devices[0]->toHandle(), &deviceDesc, size, 0u, &ptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNSUPPORTED_SIZE);
}

TEST_F(MemAllocMultiSubDeviceTestsEnabledImplicitScaling, GivenImplicitScalingEnabledWhenAllocatingDeviceMemorySubDeviceMemorySizeUsedThenExpectCorrectErrorReturned) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr = nullptr;
    size_t size = driverHandle->devices[0]->getNEODevice()->getDeviceInfo().globalMemSize;
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;

    ze_result_t res = context->allocDeviceMem(driverHandle->devices[0]->toHandle(), &deviceDesc, size, 0u, &ptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

TEST_F(MemAllocMultiSubDeviceTestsDisabledImplicitScaling, GivenImplicitScalingDisabledWhenAllocatingSharedMemorySubDeviceMemorySizeUsedThenExpectCorrectErrorReturned) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *ptr = nullptr;
    size_t size = driverHandle->devices[0]->getNEODevice()->getDeviceInfo().globalMemSize;
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;

    ze_result_t res = context->allocSharedMem(driverHandle->devices[0]->toHandle(), &deviceDesc, &hostDesc, size, 0u, &ptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNSUPPORTED_SIZE);
}

TEST_F(MemAllocMultiSubDeviceTestsEnabledImplicitScaling, GivenImplicitScalingDisabledWhenAllocatingSharedMemorySubDeviceMemorySizeUsedThenExpectCorrectErrorReturned) {
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    void *ptr = nullptr;
    size_t size = driverHandle->devices[0]->getNEODevice()->getDeviceInfo().globalMemSize;
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    deviceDesc.pNext = &relaxedSizeDesc;

    ze_result_t res = context->allocSharedMem(driverHandle->devices[0]->toHandle(), &deviceDesc, &hostDesc, size, 0u, &ptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

} // namespace ult
} // namespace L0
