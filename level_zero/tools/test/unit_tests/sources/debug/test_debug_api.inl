/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

struct DebugApiFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }
};

using DebugApiTest = Test<DebugApiFixture>;

TEST_F(DebugApiTest, givenDeviceWhenGettingDebugPropertiesThenNoFlagIsSet) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST_F(DebugApiTest, givenDeviceWhenCallingDebugAttachThenErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(DebugApiTest, givenSubDeviceWhenCallingDebugAttachThenErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession = nullptr;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = true;

    auto result = zetDebugAttach(deviceImp.toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession);
}

TEST_F(DebugApiTest, givenDeviceWhenDebugAttachIsAvaialbleThenGetPropertiesReturnsCorrectFlag) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

TEST_F(DebugApiTest, givenStateSaveAreaHeaderUnavailableWhenGettingDebugPropertiesThenAttachFlagIsNotReturned) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);
    mockBuiltIns->stateSaveAreaHeader.clear();

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST_F(DebugApiTest, givenSubDeviceWhenDebugAttachIsAvaialbleThenGetPropertiesReturnsNoFlag) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = true;

    auto result = zetDeviceGetDebugProperties(deviceImp.toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST(DebugSessionTest, givenDebugSessionWhenConvertingToAndFromHandleCorrectHandleAndPointerIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);
    L0::DebugSession *session = debugSession.get();

    zet_debug_session_handle_t debugSessionHandle = debugSession->toHandle();
    auto sessionFromHandle = L0::DebugSession::fromHandle(session);

    EXPECT_NE(nullptr, debugSessionHandle);
    EXPECT_EQ(session, sessionFromHandle);
}

TEST(DebugSessionTest, givenDebugSessionWhenGettingConnectedDeviceThenCorrectDeviceIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);
    L0::DebugSession *session = debugSession.get();

    auto device = session->getConnectedDevice();

    EXPECT_EQ(&deviceImp, device);
}

TEST(DebugSessionTest, givenDeviceWithDebugSessionWhenRemoveCalledThenSessionIsNotDeleted) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto debugSession = std::make_unique<DebugSessionMock>(config, &deviceImp);
    L0::DebugSession *session = debugSession.get();
    deviceImp.debugSession.reset(session);
    deviceImp.removeDebugSession();

    EXPECT_EQ(nullptr, deviceImp.debugSession.get());
}

TEST(DebugSessionTest, givenSubDeviceWhenCreateingSessionThenNullptrReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = true;

    ze_result_t result = ZE_RESULT_ERROR_DEVICE_LOST;
    auto session = deviceImp.createDebugSession(config, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST(DebugSessionTest, givenRootDeviceWhenCreateingSessionThenResultReturnedIsCorrect) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));

    auto osInterface = new OsInterfaceWithDebugAttach;
    osInterface->debugAttachAvailable = false;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = false;

    ze_result_t result = ZE_RESULT_ERROR_DEVICE_LOST;
    auto session = deviceImp.createDebugSession(config, result);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace L0
