/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
namespace L0 {
namespace ult {
template <int hostUsmReuseFlag = -1, int deviceUsmReuseFlag = -1, uint16_t numRootDevices = 1>
struct UsmReuseMemoryTest : public ::testing::Test {
    void SetUp() override {
        NEO::debugManager.flags.ExperimentalEnableHostAllocationCache.set(hostUsmReuseFlag);
        NEO::debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(deviceUsmReuseFlag);

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            mockProductHelpers.push_back(new MockProductHelper);
            executionEnvironment->rootDeviceEnvironments[i]->productHelper.reset(mockProductHelpers[i]);
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(),
                                                                                                                            executionEnvironment, i));
            device->deviceInfo.localMemSize = 4 * MemoryConstants::gigaByte;
            device->deviceInfo.globalMemSize = 4 * MemoryConstants::gigaByte;
            devices.push_back(std::move(device));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));

        svmAllocsManager = reinterpret_cast<MockSVMAllocsManager *>(driverHandle->svmAllocsManager);

        ze_context_handle_t hContext;
        ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        context = Context::fromHandle(hContext);
    }

    void TearDown() override {
        context->destroy();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    L0::Context *context = nullptr;
    std::vector<MockProductHelper *> mockProductHelpers;
    NEO::ExecutionEnvironment *executionEnvironment;
    NEO::MockSVMAllocsManager *svmAllocsManager;
};

using MultiDeviceReuseTest = UsmReuseMemoryTest<2, 8, 2>;

TEST_F(MultiDeviceReuseTest, givenUsmReuseEnabledWhenInitializingDriverHandleThenDoNotInitializeUsmReuse) {
    EXPECT_EQ(nullptr, svmAllocsManager->usmHostAllocationsCache.get());
    EXPECT_EQ(nullptr, svmAllocsManager->usmDeviceAllocationsCache.get());
}

using SingleDeviceReuseTest = UsmReuseMemoryTest<2, 8, 1>;

TEST_F(SingleDeviceReuseTest, givenUsmReuseEnabledWhenInitializingDriverHandleThenInitializeUsmReuse) {
    EXPECT_NE(nullptr, svmAllocsManager->usmHostAllocationsCache.get());
    EXPECT_NE(nullptr, svmAllocsManager->usmDeviceAllocationsCache.get());
}

struct SingleDeviceReuseWithoutPoolingTest : public UsmReuseMemoryTest<2, 8, 1> {
    void SetUp() override {
        NEO::debugManager.flags.EnableHostUsmAllocationPool.set(0);
        NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        UsmReuseMemoryTest<2, 8, 1>::SetUp();
    }
};

TEST_F(SingleDeviceReuseWithoutPoolingTest, givenMemAdvisedAllocationWhenAllocationIsReusedFromCacheThenMemAdviseStateIsNotInherited) {
    constexpr size_t size = MemoryConstants::pageSize;
    auto device = driverHandle->devices[0];

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ptr);

    auto allocData = svmAllocsManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    result = commandList->executeMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_UNCACHED);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(1u, device->memAdviseSharedAllocations.count(allocData));

    result = context->freeMem(ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *reusedPtr = nullptr;
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, 0u, &reusedPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(allocData, svmAllocsManager->getSVMAlloc(reusedPtr));

    EXPECT_EQ(0u, device->memAdviseSharedAllocations.count(allocData));

    result = context->freeMem(reusedPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}
} // namespace ult
} // namespace L0
