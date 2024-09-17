/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
namespace L0 {
namespace ult {
template <int hostUsmPoolFlag = -1, int deviceUsmPoolFlag = -1, int poolingVersionFlag = -1>
struct AllocUsmPoolMemoryTest : public ::testing::Test {
    void SetUp() override {
        REQUIRE_SVM_OR_SKIP(NEO::defaultHwInfo);
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(hostUsmPoolFlag);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(deviceUsmPoolFlag);
        NEO::debugManager.flags.ExperimentalUSMAllocationReuseVersion.set(poolingVersionFlag);

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            mockProductHelpers.push_back(new MockProductHelper);
            executionEnvironment->rootDeviceEnvironments[i]->productHelper.reset(mockProductHelpers[i]);
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
            if (1 == deviceUsmPoolFlag) {
                mockProductHelpers[i]->isUsmPoolAllocatorSupportedResult = true;
            }
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(),
                                                                                                                            executionEnvironment, i));
            device->deviceInfo.localMemSize = 4 * MemoryConstants::gigaByte;
            device->deviceInfo.globalMemSize = 4 * MemoryConstants::gigaByte;
            devices.push_back(std::move(device));
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
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::vector<MockProductHelper *> mockProductHelpers;
    NEO::ExecutionEnvironment *executionEnvironment;
    constexpr static auto poolAllocationThreshold = 1 * MemoryConstants::megaByte;
};

using AllocUsmHostDefaultMemoryTest = AllocUsmPoolMemoryTest<-1, -1>;

TEST_F(AllocUsmHostDefaultMemoryTest, givenDriverHandleWhenCallingAllocHostMemThenDoNotUsePool) {
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool.isInitialized());
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool.isInitialized());
    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using AllocUsmHostDisabledMemoryTest = AllocUsmPoolMemoryTest<0, -1>;

TEST_F(AllocUsmHostDisabledMemoryTest, givenDriverHandleWhenCallingAllocHostMemThenDoNotUsePool) {
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool.isInitialized());
    void *ptr = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool.isInitialized());
    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using AllocUsmHostEnabledMemoryTest = AllocUsmPoolMemoryTest<1, -1>;

TEST_F(AllocUsmHostEnabledMemoryTest, givenDriverHandleWhenCallingAllocHostMemWithVariousParametersThenUsePoolIfAllowed) {
    auto mockHostMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(&driverHandle->usmHostMemAllocPool);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool.isInitialized());
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockHostMemAllocPool->pool);

    void *ptr1Byte = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &ptr1Byte);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr1Byte);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool.isInPool(ptr1Byte));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptr1Byte));
    result = context->freeMem(ptr1Byte);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptrThreshold = nullptr;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold, 0u, &ptrThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrThreshold);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool.isInPool(ptrThreshold));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrThreshold));
    result = context->freeMem(ptrThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptrOverThreshold = nullptr;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold + 1u, 0u, &ptrOverThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrOverThreshold);
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool.isInPool(ptrOverThreshold));
    EXPECT_NE(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrOverThreshold));
    result = context->freeMem(ptrOverThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptrExportMemory = nullptr;
    ze_external_memory_export_desc_t externalMemoryDesc{};
    externalMemoryDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    externalMemoryDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    hostDesc.pNext = &externalMemoryDesc;
    result = context->allocHostMem(&hostDesc, poolAllocationThreshold, 0u, &ptrExportMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrExportMemory);
    EXPECT_FALSE(driverHandle->usmHostMemAllocPool.isInPool(ptrExportMemory));
    EXPECT_NE(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrExportMemory));
    result = context->freeMem(ptrExportMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmHostEnabledMemoryTest, givenDrmDriverModelWhenOpeningIpcHandleFromPooledAllocationThenOffsetIsApplied) {
    auto mockHostMemAllocPool = reinterpret_cast<MockUsmMemAllocPool *>(&driverHandle->usmHostMemAllocPool);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool.isInitialized());
    auto poolAllocationData = driverHandle->svmAllocsManager->getSVMAlloc(mockHostMemAllocPool->pool);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    void *pooledAllocation = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_result_t result = context->allocHostMem(&hostDesc, 1u, 0u, &pooledAllocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, pooledAllocation);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool.isInPool(pooledAllocation));
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

using AllocUsmDeviceEnabledMemoryNewVersionTest = AllocUsmPoolMemoryTest<-1, 1, 2>;

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

    void *allocation2MB = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 2 * MemoryConstants::megaByte, 0u, &allocation2MB);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation2MB);
    EXPECT_EQ(allocation2MB, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation2MB));
    EXPECT_EQ(2 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(allocation2MB));

    void *allocation4MB = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 4 * MemoryConstants::megaByte, 0u, &allocation4MB);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation4MB);
    EXPECT_EQ(nullptr, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation4MB));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(allocation4MB));

    result = context->freeMem(allocation4MB);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    void *allocation4MBRecycled = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 4 * MemoryConstants::megaByte, 0u, &allocation4MBRecycled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation4MBRecycled);
    EXPECT_EQ(allocation4MBRecycled, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation4MBRecycled));
    EXPECT_EQ(4 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(allocation4MBRecycled));
    EXPECT_EQ(allocation4MBRecycled, allocation4MB);

    result = context->freeMem(allocation4MBRecycled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    void *allocation3MBRecycled = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 3 * MemoryConstants::megaByte, 0u, &allocation3MBRecycled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation3MBRecycled);
    EXPECT_EQ(allocation3MBRecycled, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation3MBRecycled));
    EXPECT_EQ(3 * MemoryConstants::megaByte, usmMemAllocPoolsManager->getPooledAllocationSize(allocation3MBRecycled));
    auto address4MB = castToUint64(allocation4MB);
    auto address3MB = castToUint64(allocation3MBRecycled);
    EXPECT_GE(address3MB, address4MB);
    EXPECT_LT(address3MB, address4MB + MemoryConstants::megaByte);

    void *allocation2MB1B = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 2 * MemoryConstants::megaByte + 1, 0u, &allocation2MB1B);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation2MB1B);
    auto mockMemoryManager = reinterpret_cast<MockMemoryManager *>(driverHandle->getMemoryManager());
    mockMemoryManager->localMemAllocsSize[mockRootDeviceIndex] = driverHandle->getMemoryManager()->getLocalMemorySize(mockRootDeviceIndex, static_cast<uint32_t>(driverHandle->devices[0]->getNEODevice()->getDeviceBitfield().to_ulong()));
    result = context->freeMem(allocation2MB1B); // should not be recycled, because too much device memory is used
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    void *allocation2MB1BNotRecycled = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 2 * MemoryConstants::megaByte + 1, 0u, &allocation2MB1BNotRecycled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation2MB1BNotRecycled);
    EXPECT_EQ(nullptr, usmMemAllocPoolsManager->getPooledAllocationBasePtr(allocation2MB1BNotRecycled));
    EXPECT_EQ(0u, usmMemAllocPoolsManager->getPooledAllocationSize(allocation2MB1BNotRecycled));

    result = context->getIpcMemHandle(allocation2MB1BNotRecycled, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ipcData = *reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    pooledAllocationOffset = usmMemAllocPoolsManager->getOffsetInPool(allocation2MB1BNotRecycled);
    EXPECT_EQ(0u, pooledAllocationOffset);
    EXPECT_EQ(0u, ipcData.poolOffset);

    ipcPointer = nullptr;
    result = context->openIpcMemHandle(driverHandle->devices[0]->toHandle(), ipcHandle, ipcFlags, &ipcPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(addrToPtr(0x1u), ipcPointer);
    result = context->closeIpcMemHandle(addrToPtr(0x1u));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(allocation2MB1BNotRecycled);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(AllocUsmDeviceEnabledMemoryNewVersionTest, givenContextWhenNormalAllocFailsThenPoolsAreTrimmed) {
    auto usmMemAllocPoolsManager = driverHandle->devices[0]->getNEODevice()->getUsmMemAllocPoolsManager();
    ASSERT_NE(nullptr, usmMemAllocPoolsManager);
    auto mockUsmMemAllocPoolsManager = reinterpret_cast<MockUsmMemAllocPoolsManager *>(usmMemAllocPoolsManager);
    auto deviceHandle = driverHandle->devices[0]->toHandle();
    ze_device_mem_alloc_desc_t deviceAllocDesc{};
    void *allocation = nullptr;
    const auto startingPoolSize = 20 * MemoryConstants::megaByte;
    ze_result_t result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 2 * MemoryConstants::megaByte + 1, 0u, &allocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(usmMemAllocPoolsManager->isInitialized());
    result = context->freeMem(allocation);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(2 * MemoryConstants::megaByte + 1 + startingPoolSize, mockUsmMemAllocPoolsManager->totalSize);

    auto mockMemoryManager = reinterpret_cast<MockMemoryManager *>(driverHandle->getMemoryManager());
    mockMemoryManager->failInDevicePoolWithError = true;

    void *failAllocation = nullptr;
    result = context->allocDeviceMem(deviceHandle, &deviceAllocDesc, 2 * MemoryConstants::megaByte + 1, 0u, &failAllocation);
    EXPECT_EQ(nullptr, failAllocation);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);
    EXPECT_EQ(startingPoolSize, mockUsmMemAllocPoolsManager->totalSize);
}

} // namespace ult
} // namespace L0