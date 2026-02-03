/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {
template <int hostUsmPoolFlag = -1, int deviceUsmPoolFlag = -1, int usmPoolManagerFlag = -1, bool multiDevice = false, bool initDriver = true>
struct AllocUsmPoolMemoryTest : public ::testing::Test {
    void SetUp() override {
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(hostUsmPoolFlag);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(deviceUsmPoolFlag);
        NEO::debugManager.flags.EnableUsmAllocationPoolManager.set(usmPoolManagerFlag);
        NEO::debugManager.flags.EnableUsmPoolLazyInit.set(-1);

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            mockProductHelpers.push_back(new MockProductHelper);
            executionEnvironment->rootDeviceEnvironments[i]->productHelper.reset(mockProductHelpers[i]);
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
            if constexpr (deviceUsmPoolFlag > 0) {
                mockProductHelpers[i]->isDeviceUsmPoolAllocatorSupportedResult = true;
            }
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(),
                                                                                                                            executionEnvironment, i));
            device->deviceInfo.localMemSize = 4 * MemoryConstants::gigaByte;
            device->deviceInfo.globalMemSize = 4 * MemoryConstants::gigaByte;
            devices.push_back(std::move(device));
        }

        if constexpr (initDriver) {
            initDriverImp();
        }
    }

    void initDriverImp() {
        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));
        driverHandle->initUsmPooling();
        std::copy(driverHandle->devices.begin(), driverHandle->devices.end(), l0Devices);

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = static_cast<ContextImp *>(Context::fromHandle(hContext));
    }

    void TearDown() override {
        if constexpr (initDriver) {
            context->destroy();
        }
    }

    void setMaxMemoryUsed(NEO::Device &device) {
        const auto isIntegrated = device.getHardwareInfo().capabilityTable.isIntegratedDevice;
        const uint64_t deviceMemory = isIntegrated ? device.getDeviceInfo().globalMemSize : device.getDeviceInfo().localMemSize;
        auto mockMemoryManager = reinterpret_cast<MockMemoryManager *>(driverHandle->getMemoryManager());
        mockMemoryManager->localMemAllocsSize[device.getRootDeviceIndex()] = deviceMemory;
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    constexpr static uint32_t numRootDevices = multiDevice ? 2u : 1u;
    L0::ContextImp *context = nullptr;
    std::vector<MockProductHelper *> mockProductHelpers;
    NEO::ExecutionEnvironment *executionEnvironment{};
    std::vector<std::unique_ptr<NEO::Device>> devices;
    L0::Device *l0Devices[numRootDevices];
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

using AllocUsmHostDefaultMemoryTest = AllocUsmPoolMemoryTest<-1, -1, 0, true>;
TEST_F(AllocUsmHostDefaultMemoryTest, givenDriverHandleWhenCallinginitHostUsmAllocPoolThenInitIfEnabledForAllDevicesAndNoDebugger) {
    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    mockProductHelpers[0]->isHostUsmPoolAllocatorSupportedResult = false;
    mockProductHelpers[1]->isHostUsmPoolAllocatorSupportedResult = false;
    driverHandle->initHostUsmAllocPool();
    EXPECT_EQ(nullptr, driverHandle->usmHostMemAllocPool.get());

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    mockProductHelpers[0]->isHostUsmPoolAllocatorSupportedResult = true;
    mockProductHelpers[1]->isHostUsmPoolAllocatorSupportedResult = false;
    driverHandle->initHostUsmAllocPool();
    EXPECT_EQ(nullptr, driverHandle->usmHostMemAllocPool.get());

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    mockProductHelpers[0]->isHostUsmPoolAllocatorSupportedResult = true;
    mockProductHelpers[1]->isHostUsmPoolAllocatorSupportedResult = true;
    driverHandle->initHostUsmAllocPool();
    ASSERT_NE(nullptr, driverHandle->usmHostMemAllocPool.get());
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInitialized());

    for (int32_t csrType = 0; csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum); ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(csrType));
        L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
        mockProductHelpers[0]->isHostUsmPoolAllocatorSupportedResult = true;
        mockProductHelpers[1]->isHostUsmPoolAllocatorSupportedResult = true;
        driverHandle->initHostUsmAllocPool();
        if (NEO::DeviceFactory::isHwModeSelected()) {
            ASSERT_NE(nullptr, driverHandle->usmHostMemAllocPool.get());
            EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInitialized());
        } else {
            EXPECT_EQ(nullptr, driverHandle->usmHostMemAllocPool.get());
        }
    }

    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    auto debuggerL0 = DebuggerL0::create(l0Devices[1]->getNEODevice());
    executionEnvironment->rootDeviceEnvironments[1]->debugger.reset(debuggerL0.release());
    driverHandle->initHostUsmAllocPool();
    EXPECT_EQ(nullptr, driverHandle->usmHostMemAllocPool.get());
}

using AllocUsmHostDisabledMemoryTest = AllocUsmPoolMemoryTest<0, -1>;

TEST_F(AllocUsmHostDisabledMemoryTest, givenDriverHandleWhenCallingAllocHostMemThenDoNotUsePool) {
    EXPECT_EQ(nullptr, driverHandle->usmHostMemAllocPool.get());
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(nullptr, driverHandle->usmHostMemAllocPool.get());
    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using AllocUsmHostEnabledMemoryTest = AllocUsmPoolMemoryTest<1, -1, 0>;

TEST_F(AllocUsmHostEnabledMemoryTest, givenDriverHandleWhenCallingAllocHostMemWithVariousParametersThenUsePoolIfAllowed) {
    auto mockHostMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(driverHandle->usmHostMemAllocPool.get());
    ASSERT_NE(nullptr, driverHandle->usmHostMemAllocPool.get());
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInitialized());
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockHostMemAllocPool->pool);

    void *ptr1Byte = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &ptr1Byte);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1Byte);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInPool(ptr1Byte));
    EXPECT_EQ(1u, mockHostMemAllocPool->allocations.getNumAllocs());
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptr1Byte));
    result = context->freeMem(ptr1Byte);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());

    void *ptrThreshold = nullptr;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold, 0u, &ptrThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrThreshold);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInPool(ptrThreshold));
    EXPECT_EQ(1u, mockHostMemAllocPool->allocations.getNumAllocs());
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrThreshold));
    result = context->freeMem(ptrThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());

    void *ptrOverThreshold = nullptr;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold + 1u, 0u, &ptrOverThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrOverThreshold);
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool->isInPool(ptrOverThreshold));
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());
    EXPECT_NE(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrOverThreshold));
    result = context->freeMem(ptrOverThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());

    void *ptrFreeMemExt = nullptr;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold, 0u, &ptrFreeMemExt);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrFreeMemExt);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInPool(ptrFreeMemExt));
    EXPECT_EQ(1u, mockHostMemAllocPool->allocations.getNumAllocs());
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrFreeMemExt));
    ze_memory_free_ext_desc_t memFreeDesc = {};
    memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    result = context->freeMemExt(&memFreeDesc, ptrFreeMemExt);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());

    void *ptrExportMemory = nullptr;
    ze_external_memory_export_desc_t externalMemoryDesc{};
    externalMemoryDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    externalMemoryDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    hostDesc.pNext = &externalMemoryDesc;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold, 0u, &ptrExportMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrExportMemory);
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool->isInPool(ptrExportMemory));
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());
    EXPECT_NE(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrExportMemory));
    result = context->freeMem(ptrExportMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, mockHostMemAllocPool->allocations.getNumAllocs());

    void *notAllocatedPoolPtr = mockHostMemAllocPool->pool;
    EXPECT_EQ(nullptr, mockHostMemAllocPool->getPooledAllocationBasePtr(notAllocatedPoolPtr));
    result = context->freeMemExt(&memFreeDesc, notAllocatedPoolPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(AllocUsmHostEnabledMemoryTest, givenPooledAllocationWhenCallingGetMemAddressRangeThenCorrectValuesAreReturned) {
    auto pool = driverHandle->usmHostMemAllocPool.get();

    {
        auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(pool);
        void *notAllocatedPoolPtr = mockDeviceMemAllocPool->pool;
        ze_result_t result = context->getMemAddressRange(notAllocatedPoolPtr, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, result);
    }

    void *pooledAllocation = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, pooledAllocation);
    EXPECT_TRUE(pool->isInPool(pooledAllocation));

    size_t size = 0u;
    context->getMemAddressRange(pooledAllocation, nullptr, &size);
    EXPECT_GE(size, 1u);
    EXPECT_EQ(pool->getPooledAllocationSize(pooledAllocation), size);

    void *basePtr = nullptr;
    context->getMemAddressRange(pooledAllocation, &basePtr, nullptr);
    EXPECT_NE(nullptr, basePtr);
    EXPECT_EQ(pool->getPooledAllocationBasePtr(pooledAllocation), basePtr);

    result = context->freeMem(pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmHostEnabledMemoryTest, givenDrmDriverModelWhenOpeningIpcHandleFromPooledAllocationThenOffsetIsApplied) {
    ASSERT_NE(nullptr, driverHandle->usmHostMemAllocPool.get());
    auto mockHostMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(driverHandle->usmHostMemAllocPool.get());
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInitialized());
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockHostMemAllocPool->pool);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *pooledAllocation = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, pooledAllocation);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool->isInPool(pooledAllocation));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(pooledAllocation));
    const auto pooledAllocationOffset = ptrDiff(mockHostMemAllocPool->allocations.get(pooledAllocation)->address, castToUint64(mockHostMemAllocPool->pool));
    EXPECT_NE(0u, pooledAllocationOffset);

    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(pooledAllocation, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    EXPECT_EQ(pooledAllocationOffset, ipcData.poolOffset);

    ze_ipc_memory_flags_t ipcFlags{};
    void *ipcPointer = nullptr;
    result = context->openIpcMemHandle(driverHandle->devices[0]->toHandle(), ipcHandle, ipcFlags, &ipcPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ptrOffset(addrToPtr(0x1u), pooledAllocationOffset), ipcPointer);

    context->closeIpcMemHandle(addrToPtr(0x1u));

    result = context->freeMem(pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmHostEnabledMemoryTest, givenDefaultContextWhenCallingPoolCleanupThenFreePeerAllocations) {
    auto context = std::make_unique<Mock<ContextImp>>(driverHandle.get());
    auto contextHandle = context->toHandle();
    std::swap(driverHandle->defaultContext, contextHandle);
    EXPECT_EQ(0u, context->freePeerAllocationsFromAllCalled);
    driverHandle->usmHostMemAllocPool->cleanup();
    EXPECT_EQ(1u, context->freePeerAllocationsFromAllCalled);
    std::swap(driverHandle->defaultContext, contextHandle);
}

using AllocUsmDeviceDefaultSinglePoolMemoryTest = AllocUsmPoolMemoryTest<-1, -1, 0>;

TEST_F(AllocUsmDeviceDefaultSinglePoolMemoryTest, givenDeviceWhenCallingInitDeviceUsmAllocPoolThenInitIfEnabled) {
    auto neoDevice = l0Devices[0]->getNEODevice();
    {
        neoDevice->cleanupUsmAllocationPool();
        neoDevice->resetUsmAllocationPool(nullptr);
        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(nullptr);
        mockProductHelpers[0]->isDeviceUsmPoolAllocatorSupportedResult = true;
        driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
        EXPECT_NE(nullptr, neoDevice->getUsmMemAllocPool());
    }
    for (int32_t csrType = 0; csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum); ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(csrType));
        neoDevice->cleanupUsmAllocationPool();
        neoDevice->resetUsmAllocationPool(nullptr);
        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(nullptr);
        mockProductHelpers[0]->isDeviceUsmPoolAllocatorSupportedResult = true;
        driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
        EXPECT_EQ(NEO::DeviceFactory::isHwModeSelected(), nullptr != neoDevice->getUsmMemAllocPool());
    }
    {
        neoDevice->cleanupUsmAllocationPool();
        neoDevice->resetUsmAllocationPool(nullptr);
        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(nullptr);
        mockProductHelpers[0]->isDeviceUsmPoolAllocatorSupportedResult = false;
        driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
        EXPECT_EQ(nullptr, neoDevice->getUsmMemAllocPool());
    }
    {
        auto debuggerL0 = DebuggerL0::create(neoDevice);
        neoDevice->cleanupUsmAllocationPool();
        neoDevice->resetUsmAllocationPool(nullptr);
        executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(debuggerL0.release());
        mockProductHelpers[0]->isDeviceUsmPoolAllocatorSupportedResult = true;
        driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
        EXPECT_EQ(nullptr, neoDevice->getUsmMemAllocPool());
    }
}

using AllocUsmMultiDeviceDefaultSinglePoolMemoryTest = AllocUsmPoolMemoryTest<-1, -1, 0, true, false>;
TEST_F(AllocUsmMultiDeviceDefaultSinglePoolMemoryTest, givenMultiDeviceWhenInitializingDriverHandleThenDeviceUsmPoolIsInitialized) {
    mockProductHelpers[0]->isDeviceUsmPoolAllocatorSupportedResult = true;
    mockProductHelpers[1]->isDeviceUsmPoolAllocatorSupportedResult = true;
    initDriverImp();
    {
        EXPECT_NE(nullptr, l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
        EXPECT_NE(nullptr, l0Devices[1]->getNEODevice()->getUsmMemAllocPool());
    }
    context->destroy();
}

using AllocUsmMultiDeviceEnabledSinglePoolMemoryTest = AllocUsmPoolMemoryTest<0, 1, 0, true, false>;
TEST_F(AllocUsmMultiDeviceEnabledSinglePoolMemoryTest, givenMultiDeviceWhenInitializingDriverHandleThenDeviceUsmPoolInitialized) {
    mockProductHelpers[0]->isDeviceUsmPoolAllocatorSupportedResult = true;
    mockProductHelpers[1]->isDeviceUsmPoolAllocatorSupportedResult = true;
    initDriverImp();
    {
        EXPECT_NE(nullptr, l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
        EXPECT_NE(nullptr, l0Devices[1]->getNEODevice()->getUsmMemAllocPool());
    }
    context->destroy();
}

using AllocUsmDeviceDisabledSinglePoolMemoryTest = AllocUsmPoolMemoryTest<-1, 0, 0>;

TEST_F(AllocUsmDeviceDisabledSinglePoolMemoryTest, givenDeviceWhenCallingAllocDeviceMemThenDoNotUsePool) {
    EXPECT_EQ(nullptr, l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(nullptr, l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using AllocUsmDeviceEnabledSinglePoolMemoryTest = AllocUsmPoolMemoryTest<0, 1, 0>;

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenDeviceWhenCallingAllocDeviceMemWithVariousParametersThenUsePoolIfAllowed) {
    auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    ASSERT_NE(nullptr, mockDeviceMemAllocPool);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockDeviceMemAllocPool->pool);
    ASSERT_NE(nullptr, poolAllocationData);
    const bool poolIsCompressed = poolAllocationData->gpuAllocations.getDefaultGraphicsAllocation()->isCompressionEnabled();

    auto &hwInfo = l0Devices[0]->getHwInfo();
    auto &l0GfxCoreHelper = l0Devices[0]->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    if (l0GfxCoreHelper.usmCompressionSupported(hwInfo) && l0GfxCoreHelper.forceDefaultUsmCompressionSupport()) {
        EXPECT_TRUE(poolIsCompressed);
    } else {
        EXPECT_FALSE(poolIsCompressed);
    }

    {
        void *notAllocatedPoolPtr = mockDeviceMemAllocPool->pool;
        ze_result_t result = context->freeMem(notAllocatedPoolPtr);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }

    {
        void *ptr1Byte = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &ptr1Byte);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr1Byte);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(ptr1Byte));
        EXPECT_EQ(1u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptr1Byte));
        result = context->freeMem(ptr1Byte);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }

    {
        void *ptrCompressedHint = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_memory_compression_hints_ext_desc_t externalMemoryDesc{};
        externalMemoryDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        externalMemoryDesc.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;
        deviceDesc.pNext = &externalMemoryDesc;
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &ptrCompressedHint);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptrCompressedHint);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(ptrCompressedHint));
        EXPECT_EQ(1u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrCompressedHint));
        result = context->freeMem(ptrCompressedHint);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }

    {
        void *ptrCompressedHint = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_memory_compression_hints_ext_desc_t externalMemoryDesc{};
        externalMemoryDesc.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        externalMemoryDesc.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;
        deviceDesc.pNext = &externalMemoryDesc;
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &ptrCompressedHint);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptrCompressedHint);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(ptrCompressedHint));
        EXPECT_EQ(1u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrCompressedHint));
        result = context->freeMem(ptrCompressedHint);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }

    {
        void *ptrThreshold = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, poolAllocationThreshold, 0u, &ptrThreshold);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptrThreshold);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(ptrThreshold));
        EXPECT_EQ(1u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrThreshold));
        result = context->freeMem(ptrThreshold);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }

    {
        void *ptrOverThreshold = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, poolAllocationThreshold + 1u, 0u, &ptrOverThreshold);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptrOverThreshold);
        EXPECT_FALSE(mockDeviceMemAllocPool->isInPool(ptrOverThreshold));
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_NE(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrOverThreshold));
        result = context->freeMem(ptrOverThreshold);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }

    {
        void *ptrFreeMemExt = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, poolAllocationThreshold, 0u, &ptrFreeMemExt);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptrFreeMemExt);
        EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(ptrFreeMemExt));
        EXPECT_EQ(1u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrFreeMemExt));
        ze_memory_free_ext_desc_t memFreeDesc = {};
        memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
        result = context->freeMemExt(&memFreeDesc, ptrFreeMemExt);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }
    {

        void *ptrExportMemory = nullptr;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        ze_external_memory_export_desc_t externalMemoryDesc{};
        externalMemoryDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
        externalMemoryDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
        deviceDesc.pNext = &externalMemoryDesc;
        ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, poolAllocationThreshold, 0u, &ptrExportMemory);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptrExportMemory);
        EXPECT_FALSE(mockDeviceMemAllocPool->isInPool(ptrExportMemory));
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
        EXPECT_NE(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrExportMemory));
        ze_memory_free_ext_desc_t memFreeDesc = {};
        memFreeDesc.freePolicy = ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
        result = context->freeMemExt(&memFreeDesc, ptrExportMemory);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(0u, mockDeviceMemAllocPool->allocations.getNumAllocs());
    }
}

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenPooledAllocationWhenCallingGetMemAddressRangeThenCorrectValuesAreReturned) {
    auto pool = l0Devices[0]->getNEODevice()->getUsmMemAllocPool();

    {
        auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(pool);
        void *notAllocatedPoolPtr = mockDeviceMemAllocPool->pool;
        ze_result_t result = context->getMemAddressRange(notAllocatedPoolPtr, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, result);
    }

    void *pooledAllocation = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, poolAllocationThreshold, 0u, &pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, pooledAllocation);
    EXPECT_TRUE(pool->isInPool(pooledAllocation));

    size_t size = 0u;
    context->getMemAddressRange(pooledAllocation, nullptr, &size);
    EXPECT_GE(size, poolAllocationThreshold);
    EXPECT_EQ(pool->getPooledAllocationSize(pooledAllocation), size);

    void *basePtr = nullptr;
    context->getMemAddressRange(pooledAllocation, &basePtr, nullptr);
    EXPECT_NE(nullptr, basePtr);
    EXPECT_EQ(pool->getPooledAllocationBasePtr(pooledAllocation), basePtr);

    result = context->freeMem(pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenDrmDriverModelWhenOpeningIpcHandleFromPooledAllocationThenOffsetIsApplied) {
    auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    ASSERT_NE(nullptr, mockDeviceMemAllocPool);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *pooledAllocation = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &pooledAllocation);
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockDeviceMemAllocPool->pool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, pooledAllocation);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(pooledAllocation));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(pooledAllocation));
    const auto pooledAllocationOffset = ptrDiff(mockDeviceMemAllocPool->allocations.get(pooledAllocation)->address, castToUint64(mockDeviceMemAllocPool->pool));
    EXPECT_NE(0u, pooledAllocationOffset);

    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(pooledAllocation, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    EXPECT_EQ(pooledAllocationOffset, ipcData.poolOffset);

    ze_ipc_memory_flags_t ipcFlags{};
    void *ipcPointer = nullptr;
    result = context->openIpcMemHandle(driverHandle->devices[0]->toHandle(), ipcHandle, ipcFlags, &ipcPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ptrOffset(addrToPtr(0x1u), pooledAllocationOffset), ipcPointer);

    context->closeIpcMemHandle(addrToPtr(0x1u));

    result = context->freeMem(pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenDrmDriverModelWhenOpeningIpcHandleFromNotPooledAllocationThenOffsetIsNotApplied) {
    auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    ASSERT_NE(nullptr, mockDeviceMemAllocPool);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    {
        void *notAllocatedPoolPtr = mockDeviceMemAllocPool->pool;
        ze_ipc_mem_handle_t ipcHandle{};
        ze_result_t result = context->getIpcMemHandle(notAllocatedPoolPtr, &ipcHandle);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }

    void *allocation = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto mockDevice = static_cast<MockDevice *>(l0Devices[0]->getNEODevice());
    auto tempDeviceMemAllocPool = mockDevice->usmMemAllocPool.release();
    ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 1u, &allocation);
    mockDevice->usmMemAllocPool.reset(tempDeviceMemAllocPool);
    auto allocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockDeviceMemAllocPool->pool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation);
    EXPECT_FALSE(mockDeviceMemAllocPool->isInPool(allocation));
    EXPECT_NE(allocationData, driverHandle->svmAllocsManager->getSVMAlloc(allocation));

    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(allocation, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    EXPECT_EQ(0u, ipcData.poolOffset);

    ze_ipc_memory_flags_t ipcFlags{};
    void *ipcPointer = nullptr;
    result = context->openIpcMemHandle(driverHandle->devices[0]->toHandle(), ipcHandle, ipcFlags, &ipcPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(addrToPtr(0x1u), ipcPointer);

    context->closeIpcMemHandle(addrToPtr(0x1u));

    result = context->freeMem(allocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenMultiplePooledAllocationsWhenOpeningIpcHandlesAndFreeingMemoryThenTrackRefCountCorrectly) {
    auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    ASSERT_NE(nullptr, mockDeviceMemAllocPool);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());

    void *allocation1 = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &allocation1);
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockDeviceMemAllocPool->pool);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation1);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(allocation1));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(allocation1));

    void *allocation2 = nullptr;
    result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &allocation2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation2);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(allocation2));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(allocation2));

    ze_ipc_mem_handle_t ipcHandle1{};
    result = context->getIpcMemHandle(allocation1, &ipcHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    IpcMemoryData &ipcData1 = *reinterpret_cast<IpcMemoryData *>(ipcHandle1.data);
    EXPECT_EQ(mockDeviceMemAllocPool->getOffsetInPool(allocation1), ipcData1.poolOffset);

    ze_ipc_mem_handle_t ipcHandle2{};
    result = context->getIpcMemHandle(allocation1, &ipcHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    IpcMemoryData &ipcData2 = *reinterpret_cast<IpcMemoryData *>(ipcHandle2.data);
    EXPECT_EQ(mockDeviceMemAllocPool->getOffsetInPool(allocation1), ipcData2.poolOffset);

    EXPECT_TRUE(0u != ipcData1.poolOffset || 0u != ipcData2.poolOffset); // at most one pooled allocation can have offset == 0

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    ASSERT_EQ(1u, ipcHandleMap.size());
    auto ipcHandleTracking = ipcHandleMap.begin()->second;
    EXPECT_EQ(2u, ipcHandleTracking->refcnt);
    EXPECT_EQ(mockDeviceMemAllocPool->getPoolAddress(), ipcHandleTracking->ptr);

    result = context->freeMem(allocation1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, ipcHandleMap.size());
    EXPECT_EQ(1u, ipcHandleTracking->refcnt);
    result = context->freeMem(allocation2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, ipcHandleMap.size());
}

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenPooledAllocationWhenCallingResidencyOperationsThenSkipIfAllowed) {
    auto mockDeviceMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(l0Devices[0]->getNEODevice()->getUsmMemAllocPool());
    ASSERT_NE(nullptr, mockDeviceMemAllocPool);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInitialized());
    mockDeviceMemAllocPool->trackResidency = true;

    void *allocation = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &allocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(allocation));

    void *secondAlloc = nullptr;
    result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &secondAlloc);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, secondAlloc);
    EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(secondAlloc));

    auto mockMemoryOperationsHandler = static_cast<MockMemoryOperations *>(l0Devices[0]->getNEODevice()->getRootDeviceEnvironment().memoryOperationsInterface.get());
    auto expectedMakeResidentCount = mockMemoryOperationsHandler->makeResidentCalledCount;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->makeMemoryResident(l0Devices[0], allocation, 1u));
    EXPECT_EQ(++expectedMakeResidentCount, mockMemoryOperationsHandler->makeResidentCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->makeMemoryResident(l0Devices[0], secondAlloc, 1u));
    EXPECT_EQ(expectedMakeResidentCount, mockMemoryOperationsHandler->makeResidentCalledCount);

    std::unique_ptr<UsmMemAllocPool> tempPoolSwap;
    auto mockNeoDevice = reinterpret_cast<MockDevice *>(l0Devices[0]->getNEODevice());
    std::swap(tempPoolSwap, mockNeoDevice->usmMemAllocPool);

    void *nonPooledPtr = nullptr;
    result = context->allocDeviceMem(l0Devices[0], &deviceDesc, 1u, 0u, &nonPooledPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, nonPooledPtr);
    EXPECT_FALSE(mockDeviceMemAllocPool->isInPool(nonPooledPtr));
    std::swap(tempPoolSwap, mockNeoDevice->usmMemAllocPool);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->makeMemoryResident(l0Devices[0], nonPooledPtr, 1u));
    EXPECT_EQ(++expectedMakeResidentCount, mockMemoryOperationsHandler->makeResidentCalledCount);
    auto expectedEvictMemoryCallCount = mockMemoryOperationsHandler->evictCalledCount;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->evictMemory(l0Devices[0], nonPooledPtr, 1u));
    EXPECT_EQ(++expectedEvictMemoryCallCount, mockMemoryOperationsHandler->evictCalledCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->evictMemory(l0Devices[0], secondAlloc, 1u));
    EXPECT_EQ(expectedEvictMemoryCallCount, mockMemoryOperationsHandler->evictCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->evictMemory(l0Devices[0], allocation, 1u));
    EXPECT_EQ(++expectedEvictMemoryCallCount, mockMemoryOperationsHandler->evictCalledCount);

    // already evicted
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->evictMemory(l0Devices[0], allocation, 1u));
    EXPECT_EQ(expectedEvictMemoryCallCount, mockMemoryOperationsHandler->evictCalledCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(allocation));
    // not allocated pool ptr
    EXPECT_TRUE(mockDeviceMemAllocPool->isInPool(allocation));
    EXPECT_EQ(nullptr, mockDeviceMemAllocPool->getPooledAllocationBasePtr(allocation));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, context->evictMemory(l0Devices[0], allocation, 1u));
    EXPECT_EQ(expectedEvictMemoryCallCount, mockMemoryOperationsHandler->evictCalledCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->freeMem(nonPooledPtr));
}

TEST_F(AllocUsmDeviceEnabledSinglePoolMemoryTest, givenDebugFlagWhenInitializingPoolThenEnableResidencyTrackingCorrectly) {
    auto neoDevice = l0Devices[0]->getNEODevice();
    // default
    EXPECT_TRUE(neoDevice->getUsmMemAllocPool()->isTrackingResidency());
    neoDevice->cleanupUsmAllocationPool();
    neoDevice->resetUsmAllocationPool(nullptr);

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableUsmPoolResidencyTracking.set(0);
    driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
    EXPECT_FALSE(neoDevice->getUsmMemAllocPool()->isTrackingResidency());
    neoDevice->cleanupUsmAllocationPool();
    neoDevice->resetUsmAllocationPool(nullptr);

    NEO::debugManager.flags.EnableUsmPoolResidencyTracking.set(1);
    driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
    EXPECT_TRUE(neoDevice->getUsmMemAllocPool()->isTrackingResidency());
}

using AllocUsmDeviceEnabledMemoryNewVersionTest = AllocUsmPoolMemoryTest<-1, 1, -1>;

TEST_F(AllocUsmDeviceEnabledMemoryNewVersionTest, givenDebugFlagWhenInitializingPoolManagerThenEnableResidencyTrackingCorrectly) {
    auto neoDevice = l0Devices[0]->getNEODevice();

    // default
    EXPECT_TRUE(reinterpret_cast<MockUsmMemAllocPoolsManager *>(neoDevice->getUsmMemAllocPoolsManager())->trackResidency);
    neoDevice->cleanupUsmAllocationPool();
    neoDevice->resetUsmAllocationPoolManager(nullptr);

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableUsmPoolResidencyTracking.set(0);
    driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
    EXPECT_FALSE(reinterpret_cast<MockUsmMemAllocPoolsManager *>(neoDevice->getUsmMemAllocPoolsManager())->trackResidency);
    neoDevice->cleanupUsmAllocationPool();
    neoDevice->resetUsmAllocationPoolManager(nullptr);

    NEO::debugManager.flags.EnableUsmPoolResidencyTracking.set(1);
    driverHandle->initDeviceUsmAllocPool(*neoDevice, numRootDevices > 1);
    EXPECT_TRUE(reinterpret_cast<MockUsmMemAllocPoolsManager *>(neoDevice->getUsmMemAllocPoolsManager())->trackResidency);
}

TEST_F(AllocUsmDeviceEnabledMemoryNewVersionTest, givenContextWhenAllocatingAndFreeingDeviceUsmThenPoolingIsUsed) {
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    auto usmMemAllocPoolsManager = driverHandle->devices[0]->getNEODevice()->getUsmMemAllocPoolsManager();
    ASSERT_NE(nullptr, usmMemAllocPoolsManager);
    auto deviceHandle = driverHandle->devices[0]->toHandle();
    ze_device_mem_alloc_desc_t deviceAllocDesc{};
    void *allocation = nullptr;
    ze_result_t result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 1u, 0u, &allocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(usmMemAllocPoolsManager->isInitialized());
    EXPECT_EQ(allocation, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation));
    EXPECT_EQ(1u, usmMemAllocPoolsManager->getPooledAllocationSize(allocation));
    ze_ipc_mem_handle_t ipcHandle{};
    result = context->getIpcMemHandle(allocation, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    auto pooledAllocationOffset = usmMemAllocPoolsManager->getOffsetInPool(allocation);
    EXPECT_EQ(pooledAllocationOffset, ipcData.poolOffset);

    ze_ipc_memory_flags_t ipcFlags{};
    void *ipcPointer = nullptr;
    result = context->openIpcMemHandle(driverHandle->devices[0]->toHandle(), ipcHandle, ipcFlags, &ipcPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ptrOffset(addrToPtr(0x1u), pooledAllocationOffset), ipcPointer);

    result = context->closeIpcMemHandle(addrToPtr(0x1u));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = context->freeMem(allocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (auto poolInfo : PoolInfo::getPoolInfos(driverHandle->devices[0]->getGfxCoreHelper())) {
        void *allocation = nullptr;
        auto sizeToAllocate = (poolInfo.minServicedSize + poolInfo.maxServicedSize) / 2;
        result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, sizeToAllocate, 0u, &allocation);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(allocation, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation));
        auto pool = usmMemAllocPoolsManager->getPoolContainingAlloc(allocation);
        EXPECT_NE(nullptr, pool);

        auto allocationSizeReportedByPool = usmMemAllocPoolsManager->getPooledAllocationSize(allocation);
        EXPECT_GE(allocationSizeReportedByPool, poolInfo.minServicedSize);
        EXPECT_LE(allocationSizeReportedByPool, poolInfo.maxServicedSize);
        EXPECT_EQ(allocationSizeReportedByPool, sizeToAllocate);
    }

    {
        auto maxPooledSize = PoolInfo::getPoolInfos(driverHandle->devices[0]->getGfxCoreHelper()).back().maxServicedSize;
        void *allocationOverLimit = nullptr;
        result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, maxPooledSize + 1, 0u, &allocationOverLimit);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, allocationOverLimit);
        auto pool = usmMemAllocPoolsManager->getPoolContainingAlloc(allocationOverLimit);
        EXPECT_EQ(nullptr, pool);
        context->freeMem(allocationOverLimit);
    }
}
} // namespace ult
} // namespace L0
