/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace L0 {
namespace ult {

using ContextGetVirtualAddressSpaceTests = Test<DeviceFixture>;
TEST_F(ContextGetVirtualAddressSpaceTests, givenDrmDriverModelWhenCallingGetVirtualAddressSpaceIpcHandleThenUnsupportedErrorIsReturned) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_ipc_mem_handle_t ipcHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, contextImp->getVirtualAddressSpaceIpcHandle(static_cast<DeviceImp *>(device)->toHandle(), &ipcHandle));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, contextImp->putVirtualAddressSpaceIpcHandle(ipcHandle));
    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}
TEST_F(ContextGetVirtualAddressSpaceTests, givenDrmDriverModelWhenCallingSystemBarrierWithNullDeviceThenInvalidArgumentIsReturned) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_result_t barrierRes = contextImp->systemBarrier(nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, barrierRes);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}
TEST_F(ContextGetVirtualAddressSpaceTests, givenDiscreteDeviceWhenCallingSystemBarrierThenBarrierIsExecutedAndSuccessIsReturned) {
    auto &rootDeviceEnvironment = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface());
    auto drmMock = new DrmMock(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::DriverModel>(drmMock));

    uint32_t barrierMemory = 0xFFFFFFFF;

    auto mockIoctlHelper = new NEO::MockIoctlHelper(*drmMock);
    mockIoctlHelper->pciBarrierMmapReturnValue = &barrierMemory;
    drmMock->ioctlHelper.reset(mockIoctlHelper);

    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo->capabilityTable.isIntegratedDevice = false;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_result_t barrierRes = contextImp->systemBarrier(static_cast<DeviceImp *>(device)->toHandle());

    EXPECT_EQ(ZE_RESULT_SUCCESS, barrierRes);
    EXPECT_TRUE(mockIoctlHelper->pciBarrierMmapCalled);
    EXPECT_EQ(0u, barrierMemory);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}
} // namespace ult
} // namespace L0
