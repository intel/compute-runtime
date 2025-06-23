/*
 * Copyright (C) 2025 Intel Corporation
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
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
namespace L0 {
namespace ult {
template <int hostUsmReuseFlag = -1, int deviceUsmReuseFlag = -1, uint16_t numRootDevices = 1>
struct UsmReuseMemoryTest : public ::testing::Test {
    void SetUp() override {
        REQUIRE_SVM_OR_SKIP(NEO::defaultHwInfo);
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

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        svmAllocsManager = reinterpret_cast<MockSVMAllocsManager *>(driverHandle->svmAllocsManager);

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
    L0::ContextImp *context = nullptr;
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
} // namespace ult
} // namespace L0