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
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
namespace L0 {
namespace ult {
template <int hostUsmPoolFlag = -1, int deviceUsmPoolFlag = -1>
struct AllocUsmPoolMemoryTest : public ::testing::Test {
    void SetUp() override {
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(hostUsmPoolFlag);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(deviceUsmPoolFlag);

        executionEnvironment = new NEO::ExecutionEnvironment();
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
    std::vector<std::unique_ptr<NEO::Device>> devices;
    NEO::ExecutionEnvironment *executionEnvironment;
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

using AllocUsmHostEnabledMemoryTest = AllocUsmPoolMemoryTest<16, -1>;

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
    result = context->allocHostMem(&hostDesc, UsmMemAllocPool::allocationThreshold, 0u, &ptrThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptrThreshold);
    EXPECT_TRUE(driverHandle->usmHostMemAllocPool.isInPool(ptrThreshold));
    EXPECT_EQ(poolAllocationData, driverHandle->svmAllocsManager->getSVMAlloc(ptrThreshold));
    result = context->freeMem(ptrThreshold);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *ptrOverThreshold = nullptr;
    result = context->allocHostMem(&hostDesc, UsmMemAllocPool::allocationThreshold + 1u, 0u, &ptrOverThreshold);
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
    result = context->allocHostMem(&hostDesc, UsmMemAllocPool::allocationThreshold, 0u, &ptrExportMemory);
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

} // namespace ult
} // namespace L0