/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListMultiDeviceMemAdviseTest : public ::testing::Test {
  public:
    class DrmMockQueryFabricStats : public DrmMock {
      public:
        DrmMockQueryFabricStats(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}
        std::string getSysFsPciPath() override {
            return "/sys/class/drm/card1";
        }
    };

    void SetUp() override {
        DebugManagerStateRestore restorer;

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface =
                std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            auto drmMock = new DrmMockQueryFabricStats(*executionEnvironment->rootDeviceEnvironments[i]);
            drmMock->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
            osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }
        driverHandle = std::make_unique<DriverHandle>();
        driverHandle->initialize(std::move(devices));
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 1u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    std::unique_ptr<DriverHandle> driverHandle;
};

HWTEST_F(CommandListMultiDeviceMemAdviseTest, givenMultiDeviceMemAdviseWithSuccessfulCanAccessPeerTestThenAppendMemAdvisePasses) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(driverHandle->devices[0], NEO::EngineGroupType::renderCompute, 0u);

    size_t size = 10;
    void *ptr = nullptr;

    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    auto res = commandList->appendMemAdvise(driverHandle->devices[1], ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(1u, commandList->getMemAdviseOperations().size());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    free(ptr);
}

HWTEST_F(CommandListMultiDeviceMemAdviseTest, givenMultiDeviceMemAdviseWithUnsuccessfulCanAccessPeerTestThenAppendMemAdviseFails) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(driverHandle->devices[0], NEO::EngineGroupType::renderCompute, 0u);

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceZeDeviceCanAccessPerReturnValue.set(0u);
    size_t size = 10;
    void *ptr = nullptr;

    ptr = malloc(size);
    EXPECT_NE(nullptr, ptr);

    auto res = commandList->appendMemAdvise(driverHandle->devices[1], ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(0u, commandList->getMemAdviseOperations().size());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    free(ptr);
}

} // namespace ult
} // namespace L0
