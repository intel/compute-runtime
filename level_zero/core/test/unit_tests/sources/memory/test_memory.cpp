/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/memory/memory_operations_helper.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/driver_experimental/zex_memory.h"
#include "level_zero/include/level_zero/ze_intel_gpu.h"
#include "level_zero/include/level_zero/ze_stypes.h"

namespace L0 {
struct ModuleBuildLog;

namespace ult {

TEST_F(MemoryExportImportImplicitScalingTest,
       givenCallToGetIpcHandleWithNotKnownPointerThenInvalidArgumentIsReturned) {

    uint32_t value = 0;

    uint32_t numIpcHandles = 0;
    ze_result_t result = context->getIpcMemHandles(&value, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       givenCallToGetIpcHandleWithDeviceAllocationThenIpcHandleIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       givenCallToGetIpcHandleWithDeviceAllocationThenNumIpcHandlesIsUpdatedAlways) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    numIpcHandles *= 4;
    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(numIpcHandles, 2u);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct IpcMemoryImplicitScalingTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;
        debugManager.flags.EnableImplicitScaling.set(1);
        debugManager.flags.EnableWalkerPartition.set(1);
        debugManager.flags.CreateMultipleSubDevices.set(2u);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(1u, 2u, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
        context = std::make_unique<ContextIpcMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));

        auto neoDevice = device->getNEODevice();
        prevMemoryManager = driverHandle->getMemoryManager();
        currMemoryManager = new MemoryManagerIpcImplicitScalingMock(*executionEnvironment);
        driverHandle->setMemoryManager(currMemoryManager);

        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;

        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;

    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerIpcImplicitScalingMock *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextIpcMock> context;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(IpcMemoryImplicitScalingTest,
       whenGettingAllocationProperityOfAnIpcBufferWithImplicitScalingThenTheSameSubDeviceIsNotReturned) {
    uint32_t nSubDevices = 0;
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceImp->getSubDevices(&nSubDevices, nullptr));
    EXPECT_EQ(2u, nSubDevices);
    std::vector<ze_device_handle_t> subDevices(nSubDevices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceImp->getSubDevices(&nSubDevices, subDevices.data()));

    for (auto subDevice : subDevices) {
        constexpr size_t size = 1ul << 18;
        ze_device_mem_alloc_desc_t deviceDesc{};
        void *ptr = nullptr;

        EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(subDevice, &deviceDesc, size, 1ul, &ptr));
        EXPECT_NE(nullptr, ptr);

        uint32_t numIpcHandles = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandles(ptr, &numIpcHandles, nullptr));
        EXPECT_EQ(numIpcHandles, 2u);

        std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
        EXPECT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data()));

        void *ipcPtr = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, context->openIpcMemHandles(subDevice, numIpcHandles, ipcHandles.data(), 0, &ipcPtr));
        EXPECT_NE(nullptr, ipcPtr);

        ze_device_handle_t registeredDevice = nullptr;
        ze_memory_allocation_properties_t allocProp{};
        EXPECT_EQ(ZE_RESULT_SUCCESS, context->getMemAllocProperties(ipcPtr, &allocProp, &registeredDevice));
        EXPECT_EQ(ZE_MEMORY_TYPE_DEVICE, allocProp.type);
        EXPECT_NE(subDevice, registeredDevice);
        EXPECT_EQ(device->toHandle(), registeredDevice);

        EXPECT_EQ(ZE_RESULT_SUCCESS, context->closeIpcMemHandle(ipcPtr));

        for (auto &ipcHandle : ipcHandles) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, context->putIpcMemHandle(ipcHandle));
        }

        EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));
    }
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingOpenIpcHandlesWithIpcHandleThenAllocationCountIsIncremented) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usmManager = context->getDriverHandle()->getSvmAllocsManager();
    auto currentAllocationCount = usmManager->allocationsCounter.load();

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandles(device->toHandle(), numIpcHandles, ipcHandles.data(), flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto newAllocationCount = usmManager->allocationsCounter.load();
    EXPECT_GT(newAllocationCount, currentAllocationCount);
    EXPECT_EQ(usmManager->getSVMAlloc(ipcPtr)->getAllocId(), newAllocationCount);

    result = context->closeIpcMemHandle(ipcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingOpenIpcHandleWithIpcHandleAndHostMemoryTypeThenInvalidArgumentIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImplicitScaling.set(0u);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle;
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;

    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    ipcData.type = static_cast<uint8_t>(ZE_MEMORY_TYPE_HOST);
    currMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;

    result = context->openIpcMemHandle(device->toHandle(), ipcHandle, flags, &ipcPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportImplicitScalingTest,
       whenCallingGetImportFdHandleAndAllocationFailsThenNullptrIsReturned) {
    currMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(ptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t handle = 0u;
    memcpy_s(&handle,
             sizeof(handle),
             reinterpret_cast<void *>(ipcHandle.data),
             sizeof(handle));

    ze_ipc_memory_flags_t flags = {};
    NEO::GraphicsAllocation *ipcAlloc = nullptr;
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(context->getDriverHandle());
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ipcPtr = driverHandleImp->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::buffer, nullptr, &ipcAlloc, allocDataInternal);
    EXPECT_EQ(ipcPtr, nullptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using MemoryTest = Test<DeviceFixture>;

struct CompressionMemoryTest : public MemoryTest {
    void SetUp() override {
        debugManager.flags.EnableHostUsmAllocationPool.set(0);
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
        debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(0);
        MemoryTest::SetUp();
    }
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

HWTEST_F(CompressionMemoryTest, givenDeviceUsmWhenAllocatingThenEnableCompressionIfPossible) {
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedBuffers = true;
    auto &hwInfo = device->getHwInfo();
    auto &l0GfxCoreHelper = device->getL0GfxCoreHelper();
    auto &gfxCoreHelper = device->getNEODevice()->getGfxCoreHelper();

    // Default path
    {
        auto allocation = allocDeviceMem(2048);

        auto supportedByDefault = gfxCoreHelper.usmCompressionSupported(hwInfo) || (l0GfxCoreHelper.usmCompressionSupported(hwInfo) && l0GfxCoreHelper.forceDefaultUsmCompressionSupport());

        EXPECT_EQ(supportedByDefault, allocation->isCompressionEnabled());

        context->freeMem(ptr);
    }

    // Compressed hint
    {
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_EQ(gfxCoreHelper.isBufferSizeSuitableForCompression(2048), allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Compressed hint
    {
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(1);
        NEO::debugManager.flags.OverrideBufferSuitableForRenderCompression.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_TRUE(allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(-1);
        NEO::debugManager.flags.OverrideBufferSuitableForRenderCompression.set(-1);
    }

    // Compressed hint without debug flag
    {
        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_EQ(l0GfxCoreHelper.usmCompressionSupported(hwInfo), allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Uncompressed hint
    {
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(1);

        ze_external_memory_import_win32_handle_t compressionHint = {};
        compressionHint.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        compressionHint.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;

        deviceDesc.pNext = &compressionHint;

        auto allocation = allocDeviceMem(2048);

        EXPECT_EQ(gfxCoreHelper.usmCompressionSupported(hwInfo), allocation->isCompressionEnabled());

        context->freeMem(ptr);

        deviceDesc.pNext = nullptr;
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Debug flag == 0
    {
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(0);

        auto allocation = allocDeviceMem(2048);

        EXPECT_FALSE(allocation->isCompressionEnabled());

        context->freeMem(ptr);

        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(-1);
    }

    // Size restriction
    {
        NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(1);

        auto allocation = allocDeviceMem(1);

        if (!gfxCoreHelper.isBufferSizeSuitableForCompression(1) && !gfxCoreHelper.usmCompressionSupported(hwInfo)) {
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
    auto usmPool = device->getNEODevice()->getUsmMemAllocPool();
    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_NE(alloc->pageSizeForAlignment, 0u);
    EXPECT_EQ(alloc->pageSizeForAlignment, memoryProperties.pageSize);

    if (usmPool &&
        usmPool->isInPool(ptr)) {
        EXPECT_EQ(memoryProperties.id, alloc->getAllocId());
    } else {
        EXPECT_EQ(memoryProperties.id,
                  context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);
    }

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
    auto usmPool = &driverHandle->usmHostMemAllocPool;
    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_NE(alloc->pageSizeForAlignment, 0u);
    EXPECT_EQ(alloc->pageSizeForAlignment, memoryProperties.pageSize);

    if (usmPool->isInPool(ptr)) {
        EXPECT_EQ(memoryProperties.id, alloc->getAllocId());
    } else {
        EXPECT_EQ(memoryProperties.id,
                  context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);
    }

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenHostPointerMemmapSystemExtensionWhenAllocatingHostMemThenHostPtrMemoryIsUsed) {
    size_t size = 4096;
    size_t alignment = 4096;
    void *ptr = nullptr;
    auto memory = malloc(size);

    ze_external_memmap_sysmem_ext_desc_t sysMemDesc = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMMAP_SYSMEM_EXT_DESC,
                                                       nullptr, memory, size};
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.pNext = &sysMemDesc;

    ze_result_t result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memory, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    ze_device_handle_t deviceHandle;

    result = context->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);

    auto alloc = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->pageSizeForAlignment, memoryProperties.pageSize);
    EXPECT_EQ(alloc->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(), size);
    EXPECT_EQ(alloc->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBuffer(), memory);
    EXPECT_EQ(alloc->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress(), reinterpret_cast<uint64_t>(memory));

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    free(memory);
}

TEST_F(MemoryTest, givenHostPointerMemmapSystemExtensionWhenMemoryAllocationFailsThenErrorIsReturned) {
    size_t size = 4096;
    size_t alignment = 4096;
    void *ptr = nullptr;
    auto memory = malloc(size);

    ze_external_memmap_sysmem_ext_desc_t sysMemDesc = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMMAP_SYSMEM_EXT_DESC,
                                                       nullptr, memory, size};
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.pNext = &sysMemDesc;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;
    ze_result_t result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);

    free(memory);
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
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);

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
    NEO::debugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
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
    NEO::debugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
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
    NEO::debugManager.flags.ForceExtendedUSMBufferSize.set(pageSizeNumber);
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
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);

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
              context->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);

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

TEST_F(MemoryTest, whenAllocatingHostMemoryWithoutDescriptorThenCachedResourceIsCreated) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocHostMem(nullptr, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingDeviceMemoryWithoutDescriptorThenCachedResourceIsCreated) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 nullptr,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryWithoutDescriptorsThenCachedResourceWithCpuInitialPlacementIsCreated) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 nullptr,
                                                 nullptr,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);
    EXPECT_EQ(allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementCpu, 1u);
    EXPECT_EQ(allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementGpu, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingHostMemoryWithDefaultDescriptorThenCachedResourceIsCreated) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocHostMem(&defaultIntelHostMemDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingDeviceMemoryWithDefaultDescriptorThenCachedResourceIsCreated) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &defaultIntelDeviceMemDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryWithDefaultDescriptorsThenCachedResourceWithCpuInitialPlacementIsCreated) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &defaultIntelDeviceMemDesc,
                                                 &defaultIntelHostMemDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);
    EXPECT_EQ(allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementCpu, 1u);
    EXPECT_EQ(allocData->allocationFlagsProperty.allocFlags.usmInitialPlacementGpu, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryWithUseHostPtrFlagThenExternalHostPtrIsSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.flags = ZEX_HOST_MEM_ALLOC_FLAG_USE_HOST_PTR;
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.hostptr, 0x1234u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenNoSupportForDualStorageSharedMemoryWhenAllocatingSharedMemoryWithUseHostPtrFlagThenExternalHostPtrIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(0);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.flags = ZEX_HOST_MEM_ALLOC_FLAG_USE_HOST_PTR;
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.hostptr, 0x1234u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithZeroInputSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = 0;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithNegativeAttributesInputSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_HOST_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_DEVICE_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithDeviceNullptrFailureIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = 0;
    result = context->setAtomicAccessAttribute(nullptr, ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeForNonSharedMemoryAllocationFailureIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = 0;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    attr = 0;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithInvalidAllocationPtrErrorIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_ATOMICS;

    void *ptr2 = reinterpret_cast<void *>(0xface);
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr2, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithNoAtomicsThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_ATOMICS;

    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithInvalidAttributeThenFailureIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_BIT(7);

    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeWithInsufficientCapabilityThenFailureIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableConcurrentSharedCrossP2PDeviceAccess.set(false);

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getDeviceMemCapabilities() const override {
            return 0;
        }
        uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) const override {
            return 0;
        }
        uint64_t getSingleDeviceSharedMemCapabilities(bool) const override {
            return 0;
        }
    };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_HOST_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeForDeviceAccessThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getDeviceMemCapabilities() const override {
            return 3;
        }
    };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeUsingNonSystemSharedMemoryForDeviceAccessWithSystemSharedEnabledThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }

        uint64_t getDeviceMemCapabilities() const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }
    };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeUsingNonSystemSharedMemoryForDeviceAccessWithSystemSharedDisabledThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return 0;
        }

        uint64_t getDeviceMemCapabilities() const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }
    };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionWhenCallingSetAtomicAccessAttributeForDeviceAccessThenSuccessIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }

        uint64_t getDeviceMemCapabilities() const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access;

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    free(ptr);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionNotDeviceAtomicCabapleWhenCallingSetAtomicAccessAttributeForDeviceAccessThenFailureIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return 0;
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    free(ptr);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionWhenCallingSetAtomicAccessAttributeWithZeroInputSuccessIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access;

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = 0;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    free(ptr);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionWithoutConcurrentAtomicAccessCapWhenCallingSetAtomicAccessAttributeWithZeroInputFailureIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access;

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = 0;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    free(ptr);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeForHostAccessThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) const override {
            return 3;
        }
    };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_HOST_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionWhenCallingSetAtomicAccessAttributeForHostAccessThenSuccessIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }

        uint64_t getHostMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_HOST_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    free(ptr);
}

TEST_F(MemoryTest, givenSharedSystemNotHostAtomicCapableAlloctionWhenCallingSetAtomicAccessAttributeForHostAccessThenFailureIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return 0;
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_HOST_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    free(ptr);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeForSystemAccessSharedSingleThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSingleDeviceSharedMemCapabilities(bool) const override {
            return 15;
        }
    };

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionWhenCallingSetAtomicAccessAttributeForSystemAccessSharedSingleThenSuccessIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    free(ptr);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionWhenCallingSetAtomicAccessAttributeForSystemAccessSharedSingleWithoutConcurrentAtomicCapThenFailureIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess);
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess;

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    free(ptr);
}

TEST_F(MemoryTest, givenSharedSystemAlloctionNotSystemAtomicCapableWhenCallingSetAtomicAccessAttributeForSystemAccessSharedSingleTheFailureIsReturned) {

    struct MockProductHelperAtomic : NEO::ProductHelperHw<IGFX_UNKNOWN> {
        MockProductHelperAtomic() = default;
        uint64_t getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) const override {
            return 0;
        }
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto mockProductHelper = std::make_unique<MockProductHelperAtomic>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    size_t size = 10;
    void *ptr = nullptr;
    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS;
    auto result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    free(ptr);
}

TEST_F(MemoryTest, whenCallingGetAtomicAccessAttributeThenSuccessIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptr2 = reinterpret_cast<void *>(0x5678);
    result = context->allocSharedMem(device->toHandle(),
                                     &deviceDesc,
                                     &hostDesc,
                                     size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_DEVICE_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr2, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_atomic_attr_exp_flags_t attrGet = 0;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr, size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_SYSTEM_ATOMICS, attrGet);

    attrGet = 0;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr2, size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_DEVICE_ATOMICS, attrGet);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = context->freeMem(ptr2);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingSetAtomicAccessAttributeMoreThanOnceThenGetAtomicAccessAttributeReturnsLastSetting) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_atomic_attr_exp_flags_t attrGet = 0;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr, size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_ATOMICS, attrGet);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingGetAtomicAccessAttributeWithInvalidAllocationPtrErrorIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(device->toHandle(), ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_memory_atomic_attr_exp_flags_t attrGet = 0;
    void *ptr2 = reinterpret_cast<void *>(0xFACE);
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr2, size, &attrGet);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, whenCallingGetAtomicAccessAttributeWithAttributeNotSetErrorIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_atomic_attr_exp_flags_t attr = ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_NO_SYSTEM_ATOMICS;
    result = context->setAtomicAccessAttribute(nullptr, ptr, size, attr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ze_memory_atomic_attr_exp_flags_t attrGet = 0;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr, size, &attrGet);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}
// XX

struct SVMAllocsManagerAtomicAccessMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerAtomicAccessMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    NEO::AtomicAccessMode getSharedSystemAtomicAccess(NEO::Device &device, const void *ptr, const size_t size) override {
        return mode;
    }

    AtomicAccessMode mode = AtomicAccessMode::none;
};

struct SystemSharedAtomicAccessTests : public ::testing::Test {
    void SetUp() override {
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerAtomicAccessMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        if (context) {
            context->destroy();
        }
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }

    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
};

TEST_F(SystemSharedAtomicAccessTests, whenCallingGetAtomicAccessAttributeWithSystemAllocatorThenCorrectAttributeIsReturned) {

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    debugManager.flags.EnableRecoverablePageFaults.set(1u);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = UnifiedSharedMemoryFlags::access;

    size_t size = 10;
    auto ptr = std::make_unique<char[]>(size);
    ASSERT_NE(nullptr, ptr.get());

    SVMAllocsManagerAtomicAccessMock *memManager = reinterpret_cast<SVMAllocsManagerAtomicAccessMock *>(currSvmAllocsManager);

    ze_memory_atomic_attr_exp_flags_t attrGet = 0;
    memManager->mode = AtomicAccessMode::device;
    auto result = context->getAtomicAccessAttribute(device->toHandle(), ptr.get(), size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_DEVICE_ATOMICS, attrGet);

    attrGet = 0;
    memManager->mode = AtomicAccessMode::host;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr.get(), size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_HOST_ATOMICS, attrGet);

    attrGet = 0;
    memManager->mode = AtomicAccessMode::system;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr.get(), size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZE_MEMORY_ATOMIC_ATTR_EXP_FLAG_SYSTEM_ATOMICS, attrGet);

    attrGet = 0;
    memManager->mode = AtomicAccessMode::none;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr.get(), size, &attrGet);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, attrGet);

    attrGet = 0;
    memManager->mode = AtomicAccessMode::invalid;
    result = context->getAtomicAccessAttribute(device->toHandle(), ptr.get(), size, &attrGet);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(0u, attrGet);
}

TEST_F(MemoryTest, whenAllocatingHostMemoryWithUseHostPtrFlagThenExternalHostPtrIsSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.flags = ZEX_HOST_MEM_ALLOC_FLAG_USE_HOST_PTR;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.hostptr, 0x1234u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenProductWith48bForRTWhenAllocatingSharedMemoryAsRayTracingThenAllocationAddressIsIn48Bits) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_raytracing_mem_alloc_ext_desc_t rtDesc = {};

    rtDesc.stype = ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC;
    deviceDesc.pNext = &rtDesc;

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->validateAllocateProperties = [](const AllocationProperties &properties) -> void {
        EXPECT_TRUE(properties.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.hostptr & 0xffff000000000000, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

TEST_F(MemoryTest, givenKmdMigrationsAndProductWith48bForRTWhenAllocatingSharedMemoryAsRayTracingThenAllocationAddressIsIn48Bits) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(true);
    debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(true);
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_raytracing_mem_alloc_ext_desc_t rtDesc = {};

    rtDesc.stype = ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC;
    deviceDesc.pNext = &rtDesc;

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->validateAllocateProperties = [](const AllocationProperties &properties) -> void {
        EXPECT_TRUE(properties.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.hostptr & 0xffff000000000000, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

TEST_F(MemoryTest, givenProductWithNon48bForRTWhenAllocatingSharedMemoryAsRayTracingThenResourceIsNot48b) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_raytracing_mem_alloc_ext_desc_t rtDesc = {};

    rtDesc.stype = ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC;
    deviceDesc.pNext = &rtDesc;

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = false;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->validateAllocateProperties = [](const AllocationProperties &properties) -> void {
        EXPECT_FALSE(properties.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

TEST_F(MemoryTest, givenProductWith48bForRTWhenAllocatingDeviceMemoryAsRayTracingAllocationAddressIsIn48Bits) {
    size_t size = 10;
    size_t alignment = 1u;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *ptr = reinterpret_cast<void *>(0x1234);
    ze_raytracing_mem_alloc_ext_desc_t rtDesc = {};
    rtDesc.stype = ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC;
    deviceDesc.pNext = &rtDesc;

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->validateAllocateProperties = [](const AllocationProperties &properties) -> void {
        EXPECT_TRUE(properties.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.hostptr & 0xffff000000000000, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

TEST_F(MemoryTest, givenProductWithNon48bForRTWhenAllocatingDeviceMemoryAsRayTracingThenResourceIsNot48b) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_raytracing_mem_alloc_ext_desc_t rtDesc = {};

    rtDesc.stype = ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC;
    deviceDesc.pNext = &rtDesc;

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = false;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->validateAllocateProperties = [](const AllocationProperties &properties) -> void {
        EXPECT_FALSE(properties.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);

    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

TEST_F(MemoryTest, givenContextWhenGettingPitchFor2dImageThenCorrectRowPitchIsReturned) {
    size_t rowPitch = 0;
    ze_result_t result = context->getPitchFor2dImage(device->toHandle(),
                                                     4, 4,
                                                     4, &rowPitch);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(0u, rowPitch);

    rowPitch = 0;
    result = context->getPitchFor2dImage(device->toHandle(),
                                         4, 4,
                                         1024, &rowPitch);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(0u, rowPitch);
}

struct SVMAllocsManagerSharedAllocZexPointerMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerSharedAllocZexPointerMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void *createHostUnifiedMemoryAllocation(size_t size,
                                            const UnifiedMemoryProperties &memoryProperties) override {
        hostUnifiedMemoryAllocationTimes++;
        return alignedMalloc(4096u, 4096u);
    }
    void *createSharedUnifiedMemoryAllocation(size_t size,
                                              const UnifiedMemoryProperties &svmProperties,
                                              void *cmdQ) override {

        sharedUnifiedMemoryAllocationTimes++;
        return alignedMalloc(4096u, 4096u);
    }

    uint32_t hostUnifiedMemoryAllocationTimes = 0;
    uint32_t sharedUnifiedMemoryAllocationTimes = 0;
};

struct ContextZexPointerMock : public ContextImp {
    ContextZexPointerMock(L0::DriverHandleImp *driverHandle) : ContextImp(driverHandle) {}
    ze_result_t freeMem(const void *ptr) override {
        alignedFree(const_cast<void *>(ptr));
        return ZE_RESULT_SUCCESS;
    }
};

struct ZexHostPointerTests : public ::testing::Test {
    void SetUp() override {

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerSharedAllocZexPointerMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        context = std::make_unique<ContextZexPointerMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    SVMAllocsManagerSharedAllocZexPointerMock *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextZexPointerMock> context;
};

TEST_F(ZexHostPointerTests, whenAllocatingSharedMemoryWithUseHostPtrFlagThenCreateSharedUSMAlloc) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.flags = ZEX_HOST_MEM_ALLOC_FLAG_USE_HOST_PTR;
    uint32_t prevAllocCounterShared = currSvmAllocsManager->sharedUnifiedMemoryAllocationTimes;
    uint32_t prevAllocCounterHost = currSvmAllocsManager->hostUnifiedMemoryAllocationTimes;
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    uint32_t curAllocCounterShared = currSvmAllocsManager->sharedUnifiedMemoryAllocationTimes;
    uint32_t curAllocCounterHost = currSvmAllocsManager->hostUnifiedMemoryAllocationTimes;
    EXPECT_EQ(curAllocCounterShared, prevAllocCounterShared + 1);
    EXPECT_EQ(curAllocCounterHost, prevAllocCounterHost);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryTest, whenAllocatingDeviceMemoryThenAlignmentIsPassedCorrectlyAndMemoryIsAligned) {
    const size_t size = 1;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());

    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, MemoryConstants::pageSize64k));
        };
        void *ptr = nullptr;
        ze_result_t result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        result = context->freeMem(ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } while (alignment != 0);
}

TEST_F(MemoryTest, whenAllocatingHostMemoryThenAlignmentIsPassedCorrectlyAndMemoryIsAligned) {
    const size_t size = 1;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());

    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, MemoryConstants::pageSize));
        };
        void *ptr = nullptr;
        ze_result_t result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        result = context->freeMem(ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } while (alignment != 0);
}

TEST_F(MemoryTest, whenAllocatingSharedMemoryThenAlignmentIsPassedCorrectlyAndMemoryIsAligned) {
    const size_t size = 1;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());

    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, MemoryConstants::pageSize64k));
        };
        void *ptr = nullptr;
        ze_result_t result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        result = context->freeMem(ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } while (alignment != 0);
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

HWTEST2_F(MemoryTest, whenAllocatingDeviceMemoryWithEnableShareableWithoutNTHandleFlagOnNonDG1ThenShareableWithoutNTHandleIsSet, IsNotDG1) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(1);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.shareableWithoutNTHandle, 1u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MemoryTest, whenAllocatingDeviceMemoryWithEnableShareableWithoutNTHandleFlagOnDG1ThenShareableWithoutNTHandleIsNotSet, IsDG1) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(1);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.shareableWithoutNTHandle, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct SVMAllocsManagerFreeExtMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerFreeExtMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    bool freeSVMAlloc(void *ptr, bool blocking) override {
        if (blocking) {
            blockingCallsMade++;
        }
        return SVMAllocsManager::freeSVMAlloc(ptr, blocking);
    }

    bool freeSVMAllocDefer(void *ptr) override {
        deferFreeCallsMade++;
        return SVMAllocsManager::freeSVMAllocDefer(ptr);
    }

    uint32_t numDeferFreeAllocs() {
        return static_cast<uint32_t>(SVMAllocsManager::getNumDeferFreeAllocs());
    }

    uint32_t blockingCallsMade = 0u;
    uint32_t deferFreeCallsMade = 0u;
};

struct FreeExtTests : public ::testing::Test {
    void SetUp() override {
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        currSvmAllocsManager = new SVMAllocsManagerFreeExtMock(driverHandle->memoryManager);
        driverHandle->svmAllocsManager = currSvmAllocsManager;
        device = driverHandle->devices[0];

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        if (context) {
            context->destroy();
        }
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    NEO::SVMAllocsManager *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    L0::ContextImp *context = nullptr;
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
       whenFreeMemExtIsCalledWithDeferFreePolicyThenBlockingCallIsNotMade) {
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    EXPECT_EQ(0u, memManager->blockingCallsMade);
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledWithDeferFreePolicyAndInvalidPtrThenReturnInvalidArgument) {
    void *ptr = nullptr;

    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    ze_result_t result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledWithDeferFreePolicyAndAllocationNotInUseThenMemoryFreeNotDeferred) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    EXPECT_EQ(0u, memManager->blockingCallsMade);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledWithDeferFreePolicyAndAllocationInUseThenMemoryFreeDeferred) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;
    void *ptr2 = nullptr;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    EXPECT_EQ(1u, memManager->blockingCallsMade);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(2u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);
    result = context->freeMemExt(&memFreeDesc, ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(3u, memManager->deferFreeCallsMade);
}

TEST_F(FreeExtTests,
       whenFreeMemExtIsCalledMultipleTimesForSameAllocationWithDeferFreePolicyAndAllocationInUseThenMemoryFreeDeferredOnlyOnce) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);

    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(2u, memManager->deferFreeCallsMade);

    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(3u, memManager->deferFreeCallsMade);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(4u, memManager->deferFreeCallsMade);
}

TEST_F(FreeExtTests,
       whenFreeMemIsCalledWithDeferredFreeAllocationThenMemoryFreed) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    void *ptr2 = nullptr;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);
    result = context->freeMem(ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
}

TEST_F(FreeExtTests,
       whenAllocMemFailsWithDeferredFreeAllocationThenMemoryFreed) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;

    void *ptr2 = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};

    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = false;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = false;

    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(2u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;

    result = context->allocDeviceMem(device,
                                     &deviceDesc,
                                     size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = false;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = false;

    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(3u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;

    result = context->allocSharedMem(device->toHandle(),
                                     &deviceDesc,
                                     &hostDesc,
                                     size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
}

TEST_F(FreeExtTests,
       whenAllocMemFailsWithDeferredFreeAllocationThenMemoryFreedAndRetrySucceeds) {
    // does not make sense for usm pooling, disable for test
    driverHandle->usmHostMemAllocPool.cleanup();
    if (auto deviceUsmPool = neoDevice->getUsmMemAllocPool()) {
        deviceUsmPool->cleanup();
        neoDevice->usmMemAllocPool.reset(nullptr);
    }

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->singleFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->singleFailureInAllocationWithHostPointer = true;

    void *ptr2 = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};

    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;
    result = context->freeMemExt(&memFreeDesc, ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(2u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->singleFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->singleFailureInAllocationWithHostPointer = true;

    result = context->allocDeviceMem(device,
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(3u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->singleFailureInPrimaryAllocation = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->singleFailureInAllocationWithHostPointer = true;

    result = context->allocSharedMem(device->toHandle(),
                                     &deviceDesc,
                                     &hostDesc,
                                     size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    result = context->freeMemExt(&memFreeDesc, ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(4u, memManager->deferFreeCallsMade);
}

TEST_F(FreeExtTests,
       whenDestroyContextAnyRemainingDeferFreeMemoryAllocationsAreFreed) {
    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = true;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    void *ptr2 = nullptr;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);
    void *ptr3 = nullptr;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr3);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr3);
    SVMAllocsManagerFreeExtMock *memManager = reinterpret_cast<SVMAllocsManagerFreeExtMock *>(currSvmAllocsManager);
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(1u, memManager->deferFreeCallsMade);
    result = context->freeMemExt(&memFreeDesc, ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(2u, memManager->deferFreeCallsMade);
    result = context->freeMemExt(&memFreeDesc, ptr3);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(3u, memManager->numDeferFreeAllocs());
    EXPECT_EQ(3u, memManager->deferFreeCallsMade);
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->deferAllocInUse = false;
    context->destroy();
    context = nullptr;
    EXPECT_EQ(0u, memManager->numDeferFreeAllocs());
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
    SVMAllocsManagerOutOFMemoryMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void *createUnifiedMemoryAllocation(size_t size,
                                        const UnifiedMemoryProperties &svmProperties) override {
        return nullptr;
    }
};

struct OutOfMemoryTests : public ::testing::Test {
    void SetUp() override {

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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
    SVMAllocsManagerRelaxedSizeMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void *createUnifiedMemoryAllocation(size_t size,
                                        const UnifiedMemoryProperties &svmProperties) override {
        validateMemoryProperties(svmProperties);
        auto retPtr{alignedMalloc(4096u, 4096u)};

        SvmAllocationData allocData(svmProperties.getRootDeviceIndex());
        mockUnifiedMemoryAllocation.setGpuPtr(reinterpret_cast<uint64_t>(retPtr));
        mockUnifiedMemoryAllocation.setAllocationOffset(0U);
        allocData.gpuAllocations.addAllocation(&mockUnifiedMemoryAllocation);
        insertSVMAlloc(retPtr, allocData);

        return retPtr;
    }

    void *createSharedUnifiedMemoryAllocation(size_t size,
                                              const UnifiedMemoryProperties &svmProperties,
                                              void *cmdQ) override {
        validateMemoryProperties(svmProperties);
        return alignedMalloc(4096u, 4096u);
    }

    void *createHostUnifiedMemoryAllocation(size_t size,
                                            const UnifiedMemoryProperties &memoryProperties) override {
        validateMemoryProperties(memoryProperties);
        return alignedMalloc(4096u, 4096u);
    }
    std::function<void(const UnifiedMemoryProperties &)> validateMemoryProperties = [](const UnifiedMemoryProperties &properties) -> void {};

    MockGraphicsAllocation mockUnifiedMemoryAllocation{};
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
        // disable usm pooling, used svm manager mock does not create svmData
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        debugManager.flags.EnableHostUsmAllocationPool.set(0);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
    }
    NEO::SVMAllocsManager *prevSvmAllocsManager;
    SVMAllocsManagerRelaxedSizeMock *currSvmAllocsManager;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextRelaxedSizeMock> context;
    DebugManagerStateRestore restorer;
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
       givenCallToHostAllocWithLargerThanAllowedSizeOrZeroSizeAndWithoutRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);

    result = context->allocHostMem(&hostDesc,
                                   0u, alignment, &ptr);
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
    debugManager.flags.AllowUnrestrictedSize.set(1);
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
    relaxedSizeDesc.flags = {};
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
    relaxedSizeDesc.flags = {};
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
       givenCallToDeviceAllocWithLargerThanAllowedSizeOrZeroSizeAndWithoutRelaxedFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);

    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     0u, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndRelaxedFlagThenAllocationIsMade) {
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
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
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.AllowUnrestrictedSize.set(1);
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
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

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
       givenCallToDeviceAllocWithNoPhysicalMemSizeThenAllocationLargerThanGlobalMemSizeFails) {
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
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
       givenCallToSharedAllocWithNoPhysicalMemSizeThenAllocationLargerThanGlobalMemSizeFails) {
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
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

HWTEST_F(MemoryRelaxedSizeTests, givenCallToDeviceAllocWithPhysicalMemSizeThenAllocationLargerThanPhysicalMemSizeFails) {
    NEO::RAIIProductHelperFactory<MockProductHelperHw<IGFX_UNKNOWN>> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    size_t size = 1024u + 1;
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

HWTEST_F(MemoryRelaxedSizeTests, givenCallToSharedAllocWithNoPhysicalMemSizeThenAllocationLargerThanPhysicalMemSizeFails) {
    NEO::RAIIProductHelperFactory<MockProductHelperHw<IGFX_UNKNOWN>> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    size_t size = 1024 + 1;
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
       givenCallToDeviceAllocWithLargerThanAllowedSizeAndRelaxedFlagWithIncorrectFlagThenAllocationIsNotMade) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = {};
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
    relaxedSizeDesc.flags = {};
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
       givenCallToSharedAllocWithLargerThanAllowedSizeOrZeroSizeAndWithoutRelaxedFlagThenAllocationIsNotMade) {
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

    result = context->allocSharedMem(device->toHandle(),
                                     &deviceDesc,
                                     &hostDesc,
                                     0u, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(MemoryRelaxedSizeTests,
       givenCallToSharedAllocWithLargerThanAllowedSizeAndRelaxedFlagThenAllocationIsMade) {
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
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
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.AllowUnrestrictedSize.set(1);
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
    relaxedSizeDesc.flags = {};
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
       givenCallToSharedAllocWithLargerThanAllowedSizeAndRelaxedDescriptorWithWrongStypeThenUnsupportedEnumerationErrorIsReturned) {
    size_t size = device->getNEODevice()->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
    relaxedSizeDesc.flags = {};
    deviceDesc.pNext = &relaxedSizeDesc;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    EXPECT_EQ(nullptr, ptr);
}

struct ContextMemoryTests : public MemoryRelaxedSizeTests {
    void SetUp() override {
        debugManager.flags.AllowUnrestrictedSize.set(true);
        debugManager.flags.CreateMultipleSubDevices.set(4);

        MemoryRelaxedSizeTests::SetUp();

        EXPECT_EQ(4u, device->getNEODevice()->getNumGenericSubDevices());
    }

    DebugManagerStateRestore restore;
};

TEST_F(ContextMemoryTests, givenMultipleSubDevicesWhenAllocatingThenUseCorrectGlobalMemorySize) {
    size_t allocationSize = neoDevice->getDeviceInfo().globalMemSize;
    const size_t unsupportedAllocationSize = allocationSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;

    ze_result_t result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, unsupportedAllocationSize, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(nullptr, ptr);

    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, unsupportedAllocationSize, alignment, &ptr);
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
    void *importFdHandle(NEO::Device *neoDevicee, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) override {
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

        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleFailGetFdMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextFailFdMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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
       whenParsingMemoryTypeWithValidSpecifidTypeThenCorrectTypeIsReturned) {

    InternalMemoryType memoryType = InternalMemoryType::sharedUnifiedMemory;
    ze_memory_type_t usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_SHARED);

    memoryType = InternalMemoryType::deviceUnifiedMemory;
    usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_DEVICE);

    memoryType = InternalMemoryType::reservedDeviceMemory;
    usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_DEVICE);

    memoryType = InternalMemoryType::reservedHostMemory;
    usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_HOST);

    memoryType = InternalMemoryType::hostUnifiedMemory;
    usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_HOST);
}

TEST_F(MemoryExportImportFailTest,
       whenParsingMemoryTypeWithNotSpecifidTypeThenUnknownTypeIsReturned) {

    InternalMemoryType memoryType = InternalMemoryType::notSpecified;
    ze_memory_type_t usmType = L0::Context::parseUSMType(memoryType);
    EXPECT_EQ(usmType, ZE_MEMORY_TYPE_UNKNOWN);
}

TEST_F(MemoryExportImportTest,
       givenCallToDeviceAllocWithExtendedExportDescriptorAndNonSupportedFlagThenUnsupportedEnumerationIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32_KMT;
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
    auto allocData = context->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    EXPECT_NE(nullptr, allocData);
    auto allocation = allocData->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_FALSE(allocation->isCompressionEnabled());
    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptr2 = nullptr;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    deviceDesc.pNext = &extendedDesc;
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);

    result = context->freeMem(ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToHostAllocWithExtendedExportDescriptorAndSupportedFlagThenAllocationIsMade) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_external_memory_export_desc_t extendedDesc = {};
    extendedDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    hostDesc.pNext = &extendedDesc;
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptr2 = nullptr;
    extendedDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    hostDesc.pNext = &extendedDesc;
    result = context->allocHostMem(&hostDesc,
                                   size, alignment, &ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr2);

    result = context->freeMem(ptr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndNoFdHandleThenErrorIsReturned) {
    class ExportImportMockGraphicsAllocation : public NEO::MemoryAllocation {
      public:
        ExportImportMockGraphicsAllocation() : NEO::MemoryAllocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu) {}

        int peekInternalHandle(NEO::MemoryManager *memoryManager, uint64_t &handle) override {
            return -1;
        }
    };

    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD;
    extendedProperties.fd = std::numeric_limits<int>::max();

    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;
    ExportImportMockGraphicsAllocation alloc;
    ze_result_t result = context->handleAllocationExtensions(&alloc, type, &extendedProperties, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndNoFdHandleForWin32FormatThenErrorIsReturned) {
    class ExportImportMockGraphicsAllocation : public NEO::MemoryAllocation {
      public:
        ExportImportMockGraphicsAllocation() : NEO::MemoryAllocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu) {}

        int peekInternalHandle(NEO::MemoryManager *memoryManager, uint64_t &handle) override {
            return -1;
        }
    };

    ze_external_memory_export_fd_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32;
    extendedProperties.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    extendedProperties.fd = std::numeric_limits<int>::max();

    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;
    ExportImportMockGraphicsAllocation alloc;
    ze_result_t result = context->handleAllocationExtensions(&alloc, type, &extendedProperties, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesForSharedAllocationThenUnsupportedFeatureIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size,
                                                 alignment,
                                                 &ptr);
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
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesForHostAllocationThenSuccessIsReturned) {
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(extendedProperties.fd, 57);

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

struct SingleSubAllocMockGraphicsAllocation : public NEO::MockGraphicsAllocation {
    using NEO::MockGraphicsAllocation::MockGraphicsAllocation;

    uint32_t getNumHandles() override {
        return 1;
    }

    uint64_t getHandleAddressBase(uint32_t handleIndex) override {
        return 0lu;
    }

    size_t getHandleSize(uint32_t handleIndex) override {
        return 1000lu;
    }
};

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndGetSingleSubAllocation) {

    char buffer[256];
    auto alloc = new SingleSubAllocMockGraphicsAllocation(&buffer, sizeof(buffer));

    ze_memory_sub_allocations_exp_properties_t subAllocationDesc{};
    uint32_t numberOfSubAllocations = 0;
    subAllocationDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_SUB_ALLOCATIONS_EXP_PROPERTIES;
    subAllocationDesc.pCount = &numberOfSubAllocations;
    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;

    ze_result_t result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, numberOfSubAllocations);
    std::vector<ze_sub_allocation_t> subAllocationMemAllocProperties(1);
    subAllocationDesc.pSubAllocations = subAllocationMemAllocProperties.data();
    result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, numberOfSubAllocations);
    EXPECT_EQ(1000lu, subAllocationMemAllocProperties[0].size);
    EXPECT_EQ(0lu, reinterpret_cast<uint64_t>(subAllocationMemAllocProperties[0].base));
    delete alloc;
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndLargeSubAllocationCountInputThenGetSingleSubAllocation) {

    char buffer[256];
    auto alloc = new SingleSubAllocMockGraphicsAllocation(&buffer, sizeof(buffer));

    ze_memory_sub_allocations_exp_properties_t subAllocationDesc{};
    uint32_t numberOfSubAllocations = 7;
    subAllocationDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_SUB_ALLOCATIONS_EXP_PROPERTIES;
    subAllocationDesc.pCount = &numberOfSubAllocations;
    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;

    ze_result_t result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, numberOfSubAllocations);
    std::vector<ze_sub_allocation_t> subAllocationMemAllocProperties(1);
    subAllocationDesc.pSubAllocations = subAllocationMemAllocProperties.data();
    result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, numberOfSubAllocations);
    EXPECT_EQ(1000lu, subAllocationMemAllocProperties[0].size);
    EXPECT_EQ(0lu, reinterpret_cast<uint64_t>(subAllocationMemAllocProperties[0].base));
    delete alloc;
}

struct SubAllocMockGraphicsAllocation : public NEO::MockGraphicsAllocation {
    using NEO::MockGraphicsAllocation::MockGraphicsAllocation;

    uint32_t getNumHandles() override {
        return 4;
    }

    uint64_t getHandleAddressBase(uint32_t handleIndex) override {
        return (handleIndex * MemoryConstants::pageSize64k);
    }

    size_t getHandleSize(uint32_t handleIndex) override {
        return MemoryConstants::pageSize64k;
    }
};

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndGetSubAllocations) {

    char buffer[256];
    auto alloc = new SubAllocMockGraphicsAllocation(&buffer, sizeof(buffer));

    ze_memory_sub_allocations_exp_properties_t subAllocationDesc{};
    uint32_t numberOfSubAllocations = 0;
    subAllocationDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_SUB_ALLOCATIONS_EXP_PROPERTIES;
    subAllocationDesc.pCount = &numberOfSubAllocations;

    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;

    ze_result_t result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(4u, numberOfSubAllocations);
    std::vector<ze_sub_allocation_t> subAllocationMemAllocProperties(4);
    subAllocationDesc.pSubAllocations = subAllocationMemAllocProperties.data();
    result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(4u, numberOfSubAllocations);

    for (uint32_t i = 0; i < numberOfSubAllocations; i++) {
        EXPECT_EQ(MemoryConstants::pageSize64k, subAllocationMemAllocProperties[i].size);
        EXPECT_EQ(i * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(subAllocationMemAllocProperties[i].base));
    }
    delete alloc;
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndSmallSubAllocationCountInputThenGetFewerSubAllocations) {

    char buffer[256];
    auto alloc = new SubAllocMockGraphicsAllocation(&buffer, sizeof(buffer));

    ze_memory_sub_allocations_exp_properties_t subAllocationDesc{};
    uint32_t numberOfSubAllocations = 2;
    subAllocationDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_SUB_ALLOCATIONS_EXP_PROPERTIES;
    subAllocationDesc.pCount = &numberOfSubAllocations;

    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;

    ze_result_t result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, numberOfSubAllocations);
    std::vector<ze_sub_allocation_t> subAllocationMemAllocProperties(2);
    subAllocationDesc.pSubAllocations = subAllocationMemAllocProperties.data();
    result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, numberOfSubAllocations);

    for (uint32_t i = 0; i < numberOfSubAllocations; i++) {
        EXPECT_EQ(MemoryConstants::pageSize64k, subAllocationMemAllocProperties[i].size);
        EXPECT_EQ(i * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(subAllocationMemAllocProperties[i].base));
    }
    delete alloc;
}

struct NoSubAllocMockGraphicsAllocation : public NEO::MockGraphicsAllocation {
    using NEO::MockGraphicsAllocation::MockGraphicsAllocation;

    uint32_t getNumHandles() override {
        return 0;
    }

    uint64_t getHandleAddressBase(uint32_t handleIndex) override {
        return 0lu;
    }

    size_t getHandleSize(uint32_t handleIndex) override {
        return 0lu;
    }
};

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndUnsetFlagAndGetReturnError) {

    char buffer[256];
    auto alloc = new NoSubAllocMockGraphicsAllocation(&buffer, sizeof(buffer));

    ze_memory_sub_allocations_exp_properties_t subAllocationDesc{};
    uint32_t numberOfSubAllocations = 0;
    subAllocationDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_SUB_ALLOCATIONS_EXP_PROPERTIES;
    subAllocationDesc.pCount = &numberOfSubAllocations;

    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;

    ze_result_t result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, result);
    delete alloc;
}

TEST_F(MemoryExportImportTest,
       givenCallToMemAllocPropertiesWithExtendedExportPropertiesAndNullPCountAndGetReturnError) {

    char buffer[256];
    auto alloc = new SubAllocMockGraphicsAllocation(&buffer, sizeof(buffer));

    ze_memory_sub_allocations_exp_properties_t subAllocationDesc{};
    subAllocationDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_SUB_ALLOCATIONS_EXP_PROPERTIES;

    ze_memory_type_t type = ZE_MEMORY_TYPE_DEVICE;

    ze_result_t result = context->handleAllocationExtensions(alloc, type, &subAllocationDesc, driverHandle.get());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
    delete alloc;
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

HWTEST_F(MemoryTest, givenCallToGetImageAllocPropertiesWithNoBackingAllocationErrorIsReturned) {

    ze_image_allocation_ext_properties_t imageProperties = {};
    imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_EXT_PROPERTIES;

    // uninitialized, so no backing graphics allocation
    struct ImageCoreFamily<FamilyType::gfxCoreFamily> image = {};

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

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
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
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_GT(driverHandleFailGetFdMock->importFdHandleCalledTimes, 0u);
    EXPECT_EQ(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct MultipleDevicePeerAllocationTest : public ::testing::Test {
    void createModuleFromMockBinary(L0::Device *device, ModuleType type = ModuleType::user) {
        DebugManagerStateRestore restorer;
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);
        auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
        const auto &src = zebinData->storage;
        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
        moduleDesc.inputSize = src.size();

        ModuleBuildLog *moduleBuildLog = nullptr;
        ze_result_t result = ZE_RESULT_SUCCESS;
        module.reset(Module::create(device, &moduleDesc, moduleBuildLog, type, &result));
    }

    void SetUp() override {
        DebugManagerStateRestore restorer;
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);

        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
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
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }

        currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager);
        prevSvmAllocsManager = driverHandle->svmAllocsManager;
        driverHandle->svmAllocsManager = currSvmAllocsManager;
    }

    void createKernel() {
        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        kernel = std::make_unique<WhiteBox<::L0::KernelImp>>();
        kernel->setModule(module.get());
        kernel->initialize(&desc);
    }

    void TearDown() override {
        // cleanup pool before restoring svm manager
        for (auto device : driverHandle->devices) {
            device->getNEODevice()->cleanupUsmAllocationPool();
            device->getNEODevice()->resetUsmAllocationPool(nullptr);
        }
        driverHandle->svmAllocsManager = prevSvmAllocsManager;
        delete currSvmAllocsManager;
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }

    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    NEO::SVMAllocsManager *prevSvmAllocsManager = nullptr;
    NEO::SVMAllocsManager *currSvmAllocsManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextShareableMock> context;

    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::KernelImp>> kernel;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

HWTEST_F(MultipleDevicePeerAllocationTest, givenCallToMPrepareIndirectAllocationForDestructionThenOnlyValidAllocationCountsAreUpdated) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr0 = nullptr;
    L0::DeviceImp *deviceImp0 = static_cast<L0::DeviceImp *>(device0);
    auto ret = deviceImp0->getCsrForOrdinalAndIndex(&csr0, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
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
    TaskCountType prevPeekTaskCount1 = allocationData1->gpuAllocations.getGraphicsAllocation(1u)->getTaskCount(csr0->getOsContext().getContextId());
    svmManager->prepareIndirectAllocationForDestruction(allocationData1, false);
    TaskCountType postPeekTaskCount1 = allocationData1->gpuAllocations.getGraphicsAllocation(1u)->getTaskCount(csr0->getOsContext().getContextId());

    EXPECT_EQ(postPeekTaskCount1, prevPeekTaskCount1);

    ret = context->freeMem(ptr0);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
    ret = context->freeMem(ptr1);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, whenisRemoteResourceNeededIsCalledWithDifferentCombinationsOfInputsThenExpectedOutputIsReturned) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr0 = nullptr;
    L0::DeviceImp *deviceImp0 = static_cast<L0::DeviceImp *>(device0);
    auto ret = deviceImp0->getCsrForOrdinalAndIndex(&csr0, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
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

HWTEST_F(MultipleDevicePeerAllocationTest, givenCallToMakeIndirectAllocationsResidentThenOnlyValidAllocationsAreMadeResident) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr = nullptr;
    L0::DeviceImp *deviceImp1 = static_cast<L0::DeviceImp *>(device1);
    auto ret = deviceImp1->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);

    // disable device usm pooling - allocation will not be pooled but pool will be initialized
    for (auto device : driverHandle->devices) {
        device->getNEODevice()->cleanupUsmAllocationPool();
        device->getNEODevice()->resetUsmAllocationPool(nullptr);
    }

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

HWTEST_F(MultipleDevicePeerAllocationTest, givenCallToMakeInternalAllocationsResidentThenOnlyValidAllocationsAreMadeResident) {
    MemoryManagerOpenIpcMock *fixtureMemoryManager = static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    auto svmManager = driverHandle->getSvmAllocsManager();
    NEO::CommandStreamReceiver *csr = nullptr;
    L0::DeviceImp *deviceImp1 = static_cast<L0::DeviceImp *>(device1);
    auto ret = deviceImp1->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);

    // disable device usm pooling - allocation will not be pooled but pool will be initialized
    for (auto device : driverHandle->devices) {
        device->getNEODevice()->cleanupUsmAllocationPool();
        device->getNEODevice()->resetUsmAllocationPool(nullptr);
    }

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
    svmManager->makeInternalAllocationsResident(*csr, static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(1u, residentAllocations.size());
    EXPECT_EQ(residentAllocations[0]->getGpuAddress(), reinterpret_cast<uint64_t>(ptr1));

    ret = context->freeMem(ptr0);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
    ret = context->freeMem(ptr1);
    ASSERT_EQ(ret, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, whenFreeingNotKnownPointerThenInvalidArgumentIsReturned) {
    void *ptr = calloc(1, 1u);
    ze_result_t result = context->freeMem(ptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    free(ptr);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToAppendBlitFillAndImportFdHandleFailingThenInvalidArgumentIsReturned) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams;
    char pattern = 'a';
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToAppendBlitFillUsingSameDeviceThenSuccessIsReturned) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams;
    char pattern = 'a';
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToAppendBlitFillUsingDevice1ThenSuccessIsReturned) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams;
    char pattern = 'a';
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenSubDeviceAllocationPassedToAppendBlitFillUsingDevice1ThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device = driverHandle->devices[1];
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>(device);
    L0::Device *device1 = deviceImp->subDevices[0];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams;
    char pattern = 'a';
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToAppendBlitFillUsingDevice0ThenSuccessIsReturned) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams;
    char pattern = 'a';
    result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenHostPointerAllocationPassedToAppendBlitFillUsingDevice0ThenInvalidArgumentIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];

    size_t size = 1024;
    uint8_t *ptr = new uint8_t[size];

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::renderCompute, 0u);
    CmdListMemoryCopyParams copyParams;
    char pattern = 'a';
    ze_result_t result = commandList->appendBlitFill(ptr, &pattern, sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    delete[] ptr;
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToGetAllignedAllocationAndImportFdHandleFailingThenPeerAllocNotFoundReturnsTrue) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device1, false, ptr, size, false, false);
    EXPECT_EQ(nullptr, outData.alloc);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToGetAllignedAllocationUsingDevice1ThenAlignedAllocationWithPeerAllocationIsReturned) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device1, false, ptr, size, false, false);
    EXPECT_NE(outData.alignedAllocationPtr, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenSharedAllocationPassedToGetAllignedAllocationUsingDevice1ThenAlignedAllocationWithPeerAllocationIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device0->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device1, false, ptr, size, false, false);
    EXPECT_NE(outData.alignedAllocationPtr, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenDeviceAllocationPassedToGetAllignedAllocationUsingDevice0ThenAlignedAllocationWithPeerAllocationIsReturned) {
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

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device0, NEO::EngineGroupType::renderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device0, false, ptr, size, false, false);
    EXPECT_NE(outData.alignedAllocationPtr, 0u);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(MultipleDevicePeerAllocationTest, givenSharedAllocationPassedToGetAllignedAllocationUsingDevice0ThenAlignedAllocationWithPeerAllocationIsReturned) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    size_t size = 1024;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocSharedMem(device1->toHandle(),
                                                 &deviceDesc,
                                                 &hostDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device1, NEO::EngineGroupType::renderCompute, 0u);

    AlignedAllocationData outData = commandList->getAlignedAllocationData(device0, false, ptr, size, false, false);
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

    MultipleDevicePeerAllocationTest::createModuleFromMockBinary(device1);
    createKernel();

    // set argument in device 1's list with ptr from device 0: peer allocation is created
    result = kernel->setArgBuffer(0, sizeof(ptr), &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(deviceImp1->peerAllocations.getNumAllocs()), 1u);

    // set argument in device 1's list with ptr1 from device 0: another peer allocation is created
    result = kernel->setArgBuffer(0, sizeof(ptr), &ptr1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(deviceImp1->peerAllocations.getNumAllocs()), 2u);

    // set argument in device 1's list with ptr from device 0 plus offset: no new peer allocation is created
    // since a peer allocation is already available
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

    MultipleDevicePeerAllocationTest::createModuleFromMockBinary(device1);
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_NE(peerAlloc, nullptr);

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenPeerAllocationForDeviceAllocationIsRequestedWithPeerAllocDataThenPeerAllocDataIsReturned) {
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
    NEO::SvmAllocationData *peerAllocData;
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, &peerAllocData);
    EXPECT_NE(peerAlloc, nullptr);
    EXPECT_NE(peerAllocData, nullptr);

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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress, nullptr);
    EXPECT_NE(peerAlloc, nullptr);
    EXPECT_EQ(1u, deviceImp1->peerAllocations.allocations.size());

    {
        auto iter = deviceImp1->peerAllocations.allocations.find(ptr);
        EXPECT_NE(iter, deviceImp1->peerAllocations.allocations.end());
    }

    uintptr_t peerGpuAddress2 = 0u;
    peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress2, nullptr);
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
    auto peerAlloc = driverHandle->getPeerAllocation(device1, allocData, ptr, nullptr, nullptr);
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
    auto peerAlloc0 = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress0, nullptr);
    EXPECT_NE(peerAlloc0, nullptr);

    uintptr_t peerGpuAddress1 = 0u;
    auto peerAlloc1 = driverHandle->getPeerAllocation(device1, allocData, ptr, &peerGpuAddress1, nullptr);
    EXPECT_NE(peerAlloc1, nullptr);

    EXPECT_EQ(peerAlloc0, peerAlloc1);
    EXPECT_EQ(peerGpuAddress0, peerGpuAddress1);

    EXPECT_FALSE(peerAlloc0->isCompressionEnabled());
    EXPECT_FALSE(peerAlloc1->isCompressionEnabled());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MultipleDevicePeerAllocationTest,
       whenGettingAllocationProperityOfAnIpcBufferThenTheSameSubDeviceIsReturned) {
    for (auto l0Device : driverHandle->devices) {
        uint32_t nSubDevices = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, l0Device->getSubDevices(&nSubDevices, nullptr));
        EXPECT_EQ(numSubDevices, nSubDevices);
        std::vector<ze_device_handle_t> subDevices(nSubDevices);
        EXPECT_EQ(ZE_RESULT_SUCCESS, l0Device->getSubDevices(&nSubDevices, subDevices.data()));

        for (auto subDevice : subDevices) {
            constexpr size_t size = 1ul << 18;
            ze_device_mem_alloc_desc_t deviceDesc{};
            void *ptr = nullptr;

            EXPECT_EQ(ZE_RESULT_SUCCESS, context->allocDeviceMem(subDevice, &deviceDesc, size, 1ul, &ptr));
            EXPECT_NE(nullptr, ptr);

            uint32_t numIpcHandles = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandles(ptr, &numIpcHandles, nullptr));
            EXPECT_EQ(numIpcHandles, 2u);

            std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
            EXPECT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data()));

            void *ipcPtr = nullptr;
            EXPECT_EQ(ZE_RESULT_SUCCESS, context->openIpcMemHandles(subDevice, numIpcHandles, ipcHandles.data(), 0, &ipcPtr));
            EXPECT_NE(nullptr, ipcPtr);

            ze_device_handle_t registeredDevice = nullptr;
            ze_memory_allocation_properties_t allocProp{};
            EXPECT_EQ(ZE_RESULT_SUCCESS, context->getMemAllocProperties(ipcPtr, &allocProp, &registeredDevice));
            EXPECT_EQ(ZE_MEMORY_TYPE_DEVICE, allocProp.type);
            EXPECT_EQ(subDevice, registeredDevice);

            EXPECT_EQ(ZE_RESULT_SUCCESS, context->closeIpcMemHandle(ipcPtr));

            for (auto &ipcHandle : ipcHandles) {
                EXPECT_EQ(ZE_RESULT_SUCCESS, context->putIpcMemHandle(ipcHandle));
            }

            EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(ptr));
        }
    }
}

struct MemoryFailedOpenIpcHandleTest : public ::testing::Test {
    void SetUp() override {

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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

struct MemoryFailedOpenIpcHandleImplicitScalingTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;
        debugManager.flags.EnableImplicitScaling.set(1);

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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

TEST_F(MemoryFailedOpenIpcHandleImplicitScalingTest,
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

    uint32_t numIpcHandles = 0;
    result = context->getIpcMemHandles(ptr, &numIpcHandles, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::vector<ze_ipc_mem_handle_t> ipcHandles(numIpcHandles);
    result = context->getIpcMemHandles(ptr, &numIpcHandles, ipcHandles.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_ipc_memory_flags_t flags = {};
    void *ipcPtr;
    result = context->openIpcMemHandles(device->toHandle(), numIpcHandles, ipcHandles.data(), flags, &ipcPtr);
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

struct MemoryBitfieldTest : testing::Test {
    void SetUp() override {

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
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
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &ptr);
    EXPECT_EQ(neoDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
}

TEST(MemoryBitfieldTests, givenDeviceWithValidBitfieldWhenAllocatingSharedMemoryThenPassProperBitfield) {
    DebugManagerStateRestore restorer;
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (size_t i = 0; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();

        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
    }
    executionEnvironment->calculateMaxOsContextCount();

    auto memoryManager = new NEO::MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    NEO::Device *neoDevice0 = NEO::Device::create<RootDevice>(executionEnvironment, 0u);
    debugManager.flags.CreateMultipleSubDevices.set(4);

    executionEnvironment->calculateMaxOsContextCount();
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
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
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
    driverHandle->usmHostMemAllocPool.cleanup();
    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               4096u, 0u, &ptr);
    EXPECT_EQ(static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount, numRootDevices);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    context->freeMem(ptr);
}

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemAndFailingOnCreatingGraphicsAllocationThenNullIsReturned) {
    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInPrimaryAllocation = true;

    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               4096u, 0u, &ptr);
    EXPECT_EQ(static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount, 1u);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(AllocHostMemoryTest,
       whenCallingAllocHostMemAndFailingOnCreatingGraphicsAllocationWithHostPointerThenNullIsReturned) {
    driverHandle->usmHostMemAllocPool.cleanup();
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->isMockHostMemoryManager = true;
    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->forceFailureInAllocationWithHostPointer = true;

    void *ptr = nullptr;

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc,
                                               4096u, 0u, &ptr);
    EXPECT_EQ(static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->allocateGraphicsMemoryWithPropertiesCount, numRootDevices);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(nullptr, ptr);
}

using ContextMemoryTest = Test<DeviceFixture>;

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
    NEO::MemoryOperationsStatus status = NEO::MemoryOperationsStatus::success;
    ze_result_t res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    status = NEO::MemoryOperationsStatus::failed;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_DEVICE_LOST);

    status = NEO::MemoryOperationsStatus::memoryNotFound;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    status = NEO::MemoryOperationsStatus::outOfMemory;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);

    status = NEO::MemoryOperationsStatus::unsupported;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    status = NEO::MemoryOperationsStatus::deviceUninitialized;
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNINITIALIZED);

    status = static_cast<NEO::MemoryOperationsStatus>(static_cast<uint32_t>(NEO::MemoryOperationsStatus::deviceUninitialized) + 1);
    res = changeMemoryOperationStatusToL0ResultType(status);
    EXPECT_EQ(res, ZE_RESULT_ERROR_UNKNOWN);
}

using ImportFdUncachedTests = MemoryOpenIpcHandleTest;

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithUncachedFlagsThenLocallyUncachedResourceIsSet) {
    ze_ipc_memory_flags_t flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    uint64_t handle = 1;
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ptr = driverHandle->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::buffer, nullptr, nullptr, allocDataInternal);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithUncachedIpcFlagsThenLocallyUncachedResourceIsSet) {
    ze_ipc_memory_flags_t flags = ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED;
    uint64_t handle = 1;
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ptr = driverHandle->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::buffer, nullptr, nullptr, allocDataInternal);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithBothUncachedFlagsThenLocallyUncachedResourceIsSet) {
    ze_ipc_memory_flags_t flags = static_cast<ze_ipc_memory_flags_t>(ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) |
                                  static_cast<ze_ipc_memory_flags_t>(ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED);
    uint64_t handle = 1;
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ptr = driverHandle->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::buffer, nullptr, nullptr, allocDataInternal);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 1u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithoutUncachedFlagsThenLocallyUncachedResourceIsNotSet) {
    ze_ipc_memory_flags_t flags = {};
    uint64_t handle = 1;
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ptr = driverHandle->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::buffer, nullptr, nullptr, allocDataInternal);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_EQ(allocData->allocationFlagsProperty.flags.locallyUncachedResource, 0u);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithHostBufferMemoryAllocationTypeThenHostUnifiedMemoryIsSet) {
    ze_ipc_memory_flags_t flags = {};
    uint64_t handle = 1;
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ptr = driverHandle->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::bufferHostMemory, nullptr, nullptr, allocDataInternal);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(allocData->memoryType, InternalMemoryType::hostUnifiedMemory);

    context->freeMem(ptr);
}

TEST_F(ImportFdUncachedTests,
       givenCallToImportFdHandleWithBufferMemoryAllocationTypeThenDeviceUnifiedMemoryIsSet) {
    ze_ipc_memory_flags_t flags = {};
    uint64_t handle = 1;
    NEO::SvmAllocationData allocDataInternal(device->getNEODevice()->getRootDeviceIndex());
    void *ptr = driverHandle->importFdHandle(device->getNEODevice(), flags, handle, NEO::AllocationType::buffer, nullptr, nullptr, allocDataInternal);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    EXPECT_NE(allocData, nullptr);
    EXPECT_EQ(allocData->memoryType, InternalMemoryType::deviceUnifiedMemory);

    context->freeMem(ptr);
}

struct SVMAllocsManagerSharedAllocFailMock : public NEO::SVMAllocsManager {
    SVMAllocsManagerSharedAllocFailMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void *createSharedUnifiedMemoryAllocation(size_t size,
                                              const UnifiedMemoryProperties &svmProperties,
                                              void *cmdQ) override {
        return nullptr;
    }
};

struct SharedAllocFailTests : public ::testing::Test {
    void SetUp() override {

        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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
    SVMAllocsManagerSharedAllocMultiDeviceMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
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
    bool isShareableMemory(const void *pNext, bool exportableMemory, NEO::Device *neoDevice, bool shareableWithoutNTHandle) override {
        return true;
    }
};

struct SharedAllocMultiDeviceTests : public ::testing::Test {
    void SetUp() override {

        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
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
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
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

        debugManager.flags.EnableWalkerPartition.set(enableWalkerPartition);
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
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
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
    }

    void TearDown() override {
        // cleanup pool before restoring svm manager
        for (auto device : driverHandle->devices) {
            device->getNEODevice()->cleanupUsmAllocationPool();
            device->getNEODevice()->resetUsmAllocationPool(nullptr);
        }
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

class ExportImportMockGraphicsAllocation : public NEO::MemoryAllocation {
  public:
    ExportImportMockGraphicsAllocation() : NEO::MemoryAllocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu) {}

    int peekInternalHandle(NEO::MemoryManager *memoryManager, uint64_t &handle) override {
        return -1;
    }
};

class MockSharedHandleMemoryManager : public MockMemoryManager {
  public:
    using MockMemoryManager::MockMemoryManager;

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        if (failOnCreateGraphicsAllocationFromSharedHandle) {
            return nullptr;
        }

        return MockMemoryManager::createGraphicsAllocationFromSharedHandle(osHandleData, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
    }

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        if (failPeekInternalHandle) {
            return new ExportImportMockGraphicsAllocation();
        }
        return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }

    bool failOnCreateGraphicsAllocationFromSharedHandle = false;
    bool failPeekInternalHandle = false;
};

struct MultipleDevicePeerImageTest : public ::testing::Test {
    void SetUp() override {
        DebugManagerStateRestore restorer;

        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        bool enableLocalMemory = gfxCoreHelper.getEnableLocalMemory(*defaultHwInfo);
        bool aubUsage = (testMode == TestMode::aubTests) || (testMode == TestMode::aubTestsWithTbx);
        deviceFactoryMemoryManager = new MockSharedHandleMemoryManager(false, enableLocalMemory, aubUsage, *executionEnvironment);
        executionEnvironment->memoryManager.reset(deviceFactoryMemoryManager);
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<DriverHandleImp>();
        driverHandle->initialize(std::move(devices));

        context = std::make_unique<ContextShareableMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        for (auto i = 0u; i < numRootDevices; i++) {
            auto device = driverHandle->devices[i];
            context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
            auto neoDevice = device->getNEODevice();
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        }
    }

    void TearDown() override {}

    NEO::MemoryManager *deviceFactoryMemoryManager = nullptr;

    NEO::SVMAllocsManager *prevSvmAllocsManager = nullptr;
    NEO::SVMAllocsManager *currSvmAllocsManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<ContextShareableMock> context;

    const uint32_t numRootDevices = 2u;
    const uint32_t numSubDevices = 2u;
};

using ImageSupport = IsGen12LP;

HWTEST2_F(MultipleDevicePeerImageTest,
          whenisRemoteImageNeededIsCalledWithDifferentCombinationsOfInputsThenExpectedOutputIsReturned,
          ImageSupport) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

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

    L0::Image *image0;
    auto result = Image::create(productFamily, device0, &zeDesc, &image0);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    L0::Image *image1;
    result = Image::create(productFamily, device1, &zeDesc, &image1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    bool isNeeded = driverHandle->isRemoteImageNeeded(image0, device0);
    EXPECT_FALSE(isNeeded);

    isNeeded = driverHandle->isRemoteImageNeeded(image1, device0);
    EXPECT_TRUE(isNeeded);

    isNeeded = driverHandle->isRemoteImageNeeded(image0, device1);
    EXPECT_TRUE(isNeeded);

    isNeeded = driverHandle->isRemoteImageNeeded(image1, device1);
    EXPECT_FALSE(isNeeded);

    result = image0->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = image1->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerImageTest,
          givenRemoteImageAllocationsPassedToAppendImageCopyCallsUsingDevice0ThenSuccessIsReturned,
          ImageSupport) {
    const ze_command_queue_desc_t queueDesc = {};
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    L0::Image *image0Src;
    L0::Image *image0Dst;
    auto result = Image::create(productFamily, device0, &desc, &image0Src);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = Image::create(productFamily, device0, &desc, &image0Dst);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::Image *image1Src;
    L0::Image *image1Dst;
    result = Image::create(productFamily, device1, &desc, &image1Src);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = Image::create(productFamily, device1, &desc, &image1Dst);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    device0->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device0,
                                                                               &queueDesc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    CmdListMemoryCopyParams copyParams = {};
    result = commandList0->appendImageCopy(image0Dst->toHandle(), image1Src->toHandle(), nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList0->appendImageCopy(image1Dst->toHandle(), image0Src->toHandle(), nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList0->appendImageCopyFromMemory(image1Dst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList0->appendImageCopyToMemory(dstPtr, image1Src->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = image0Src->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = image0Dst->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = image1Src->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = image1Dst->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(MultipleDevicePeerImageTest,
          givenRemoteImageAllocationsPassedToAppendImageCopyCallsUsingDevice0WithFailingSharedHandleAllocationsThenErrorIsReturned,
          ImageSupport) {
    MockSharedHandleMemoryManager *fixtureMemoryManager = static_cast<MockSharedHandleMemoryManager *>(deviceFactoryMemoryManager);
    fixtureMemoryManager->failOnCreateGraphicsAllocationFromSharedHandle = true;

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    L0::Image *image0Src;
    L0::Image *image0Dst;
    auto result = Image::create(productFamily, device0, &desc, &image0Src);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = Image::create(productFamily, device0, &desc, &image0Dst);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::Image *image1Src;
    L0::Image *image1Dst;
    result = Image::create(productFamily, device1, &desc, &image1Src);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = Image::create(productFamily, device1, &desc, &image1Dst);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    device0->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    const ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device0,
                                                                               &queueDesc,
                                                                               false,
                                                                               NEO::EngineGroupType::copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);
    CmdListMemoryCopyParams copyParams = {};
    result = commandList0->appendImageCopy(image0Dst->toHandle(), image1Src->toHandle(), nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    result = commandList0->appendImageCopy(image1Dst->toHandle(), image0Src->toHandle(), nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    result = commandList0->appendImageCopyFromMemory(image1Dst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);
    result = commandList0->appendImageCopyToMemory(dstPtr, image1Src->toHandle(), nullptr, nullptr, 0, nullptr, copyParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    result = image0Src->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = image0Dst->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = image1Src->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    result = image1Dst->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    static_cast<L0::ult::CommandList *>(commandList0.get())->getCsr(false)->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device0->getNEODevice());
}

HWTEST2_F(MultipleDevicePeerImageTest,
          givenPeekInternalHandleFailsThenGetPeerImageReturnsNullptr,
          ImageSupport) {
    MockSharedHandleMemoryManager *fixtureMemoryManager = static_cast<MockSharedHandleMemoryManager *>(deviceFactoryMemoryManager);
    fixtureMemoryManager->failPeekInternalHandle = true;

    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;

    L0::Image *image;
    auto result = Image::create(productFamily, device1, &desc, &image);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::Image *peerImage = nullptr;
    result = driverHandle->getPeerImage(device0, image, &peerImage);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(peerImage, nullptr);

    result = image->destroy();
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryRelaxedSizeTests, givenMultipleExtensionsPassedToCreateSharedMemThenAllExtensionsAreParsed) {
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironmentRef();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->is48bResourceNeededForRayTracingResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    currSvmAllocsManager->validateMemoryProperties = [](const SVMAllocsManager::UnifiedMemoryProperties &memoryProperties) -> void {
        EXPECT_TRUE(memoryProperties.allocationFlags.flags.resource48Bit);
    };

    std::swap(rootDeviceEnvironment.productHelper, productHelper);
    size_t size = neoDevice->getDeviceInfo().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc = {};
    relaxedSizeDesc.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxedSizeDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;

    ze_raytracing_mem_alloc_ext_desc_t rtDesc = {};

    rtDesc.stype = ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC;

    {
        deviceDesc.pNext = &relaxedSizeDesc;
        rtDesc.pNext = nullptr;
        relaxedSizeDesc.pNext = &rtDesc;
        ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                     &deviceDesc,
                                                     &hostDesc,
                                                     size, alignment, &ptr);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);

        result = context->freeMem(ptr);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    }
    {
        deviceDesc.pNext = &rtDesc;
        rtDesc.pNext = &relaxedSizeDesc;
        relaxedSizeDesc.pNext = nullptr;
        ze_result_t result = context->allocSharedMem(device->toHandle(),
                                                     &deviceDesc,
                                                     &hostDesc,
                                                     size, alignment, &ptr);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);

        result = context->freeMem(ptr);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    }
    std::swap(rootDeviceEnvironment.productHelper, productHelper);
}

} // namespace ult
} // namespace L0
