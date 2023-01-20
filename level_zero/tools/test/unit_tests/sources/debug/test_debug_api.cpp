/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/include/zet_intel_gpu_debug.h"
#include "level_zero/tools/source/debug/debug_handlers.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

namespace L0 {
namespace ult {

struct DebugApiFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
        mockBuiltins = new MockBuiltins();
        mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltins);
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    MockBuiltins *mockBuiltins = nullptr;
};

using DebugApiTest = Test<DebugApiFixture>;
using MultiTileDebugApiTest = Test<MultipleDevicesWithCustomHwInfo>;

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

TEST_F(DebugApiTest, givenDebugAttachSupportedInOsWhenCallingDebugAttachThenSuccessIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    zet_debug_session_handle_t debugSession = nullptr;
    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);

    zet_debug_session_handle_t debugSession2 = nullptr;
    result = zetDebugAttach(device->toHandle(), &config, &debugSession2);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(debugSession, debugSession2);
}

TEST_F(DebugApiTest, givenDebugSessionWithPidWhenCallingDebugAttachForOtherPidThenErrorUnsupportedIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    auto sessionMock = new MockDebugSession(config, device, true);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    zet_debug_session_handle_t debugSession = nullptr;
    auto result = zetDebugAttach(device->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);

    zet_debug_session_handle_t debugSession2 = nullptr;
    config.pid = 0x1111;
    result = zetDebugAttach(device->toHandle(), &config, &debugSession2);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession2);
}

TEST_F(MultiTileDebugApiTest, givenDebugSessionWithPidWhenCallingDebugAttachForOtherPidThenErrorUnsupportedIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    L0::Device *device = driverHandle->devices[0];
    L0::DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    auto neoDevice = device->getNEODevice();
    auto osInterface = new OsInterfaceWithDebugAttach;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
    auto subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();

    auto sessionMock = new MockDebugSession(config, device, false);
    sessionMock->initialize();
    deviceImp->setDebugSession(sessionMock);

    zet_debug_session_handle_t debugSession = nullptr;
    auto result = zetDebugAttach(subDevice0->toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession);

    zet_debug_session_handle_t debugSession2 = nullptr;
    config.pid = 0x1111;
    result = zetDebugAttach(subDevice1->toHandle(), &config, &debugSession2);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(nullptr, debugSession2);
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

using isDebugSupportedProduct = IsWithinProducts<IGFX_DG1, IGFX_PVC>;
HWTEST2_F(DebugApiTest, givenDeviceWhenDebugAttachIsAvaialbleThenGetPropertiesReturnsCorrectFlag, isDebugSupportedProduct) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH, debugProperties.flags);
}

using isDebugNotSupportedProduct = IsNotWithinProducts<IGFX_DG1, IGFX_PVC>;
HWTEST2_F(DebugApiTest, givenDeviceWhenDebugIsNotSupportedThenGetPropertiesReturnsCorrectFlag, isDebugNotSupportedProduct) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST_F(DebugApiTest, givenStateSaveAreaHeaderUnavailableWhenGettingDebugPropertiesThenAttachFlagIsNotReturned) {
    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);
    mockBuiltins->stateSaveAreaHeader.clear();

    auto result = zetDeviceGetDebugProperties(device->toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST_F(DebugApiTest, givenTileAttachedDisabledAndSubDeviceWhenDebugAttachIsAvaialbleThenGetPropertiesReturnsNoFlag) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(0);

    zet_device_debug_properties_t debugProperties = {};
    debugProperties.flags = ZET_DEVICE_DEBUG_PROPERTY_FLAG_FORCE_UINT32;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OsInterfaceWithDebugAttach);

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = true;

    auto result = zetDeviceGetDebugProperties(deviceImp.toHandle(), &debugProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, debugProperties.flags);
}

TEST_F(DebugApiTest, givenDeviceWithDebugSessionWhenDebugAttachIsCalledThenSessionHandleIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.debugSession.reset(new DebugSessionMock(config, &deviceImp));

    zet_debug_session_handle_t debugSession = nullptr;
    auto result = zetDebugAttach(deviceImp.toHandle(), &config, &debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(deviceImp.debugSession->toHandle(), debugSession);
}

TEST_F(DebugApiTest, givenDeviceWithDebugSessionWhenDebugDetachIsCalledThenSuccessIsReturnedAndSessionDeleted) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.debugSession.reset(new DebugSessionMock(config, &deviceImp));

    zet_debug_session_handle_t debugSession = deviceImp.debugSession->toHandle();
    auto result = zetDebugDetach(debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, deviceImp.debugSession);
}

TEST_F(DebugApiTest, givenDeviceWithoutSessionWhenDebugDetachIsCalledThenSuccessIsReturned) {
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    zet_debug_session_handle_t debugSession = 0;
    auto result = zetDebugDetach(debugSession);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(nullptr, deviceImp.debugSession);
}

TEST_F(DebugApiTest, WhenUnsupportedFunctionCalledThenErrorIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.debugSession.reset(new DebugSessionMock(config, &deviceImp));

    zet_debug_session_handle_t session = deviceImp.debugSession->toHandle();

    auto result = L0::DebugApiHandlers::debugReadEvent(session, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    ze_device_thread_t thread = {};
    result = L0::DebugApiHandlers::debugInterrupt(session, thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugResume(session, thread);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugReadMemory(session, thread, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugWriteMemory(session, thread, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugAcknowledgeEvent(session, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    uint32_t type = {0};
    result = L0::DebugApiHandlers::debugReadRegisters(session, thread, type, 0, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

    result = L0::DebugApiHandlers::debugWriteRegisters(session, thread, type, 0, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_F(DebugApiTest, givenNullCountPointerWhenGetRegisterSetPropertiesCalledThenInvalidNullPointerIsReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, zetDebugGetRegisterSetProperties(device->toHandle(), nullptr, nullptr));
}

TEST_F(DebugApiTest, givenZeroCountWhenGetRegisterSetPropertiesCalledThenCorrectCountIsSet) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(device->toHandle(), &count, nullptr));
    EXPECT_EQ(12u, count);
}

TEST_F(DebugApiTest, givenGetRegisterSetPropertiesCalledAndExtraSpaceIsProvidedThenCorrectPropertiesReturned) {
    uint32_t count = 100;
    std::vector<zet_debug_regset_properties_t> regsetProps(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(device->toHandle(), &count, regsetProps.data()));
    EXPECT_EQ(12u, count);
}

TEST_F(DebugApiTest, givenNoStateSaveHeaderWhenGettingRegSetPropertiesThenZeroCountIsReturned) {
    uint32_t count = 10;
    std::vector<zet_debug_regset_properties_t> regsetProps(count);

    mockBuiltins->stateSaveAreaHeader.clear();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(device->toHandle(), &count, regsetProps.data()));
    EXPECT_EQ(0u, count);
}

TEST_F(DebugApiTest, givenNonZeroCountAndNullRegsetPointerWhenGetRegisterSetPropertiesCalledTheniInvalidNullPointerIsReturned) {
    uint32_t count = 12;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, zetDebugGetRegisterSetProperties(device->toHandle(), &count, nullptr));
}

TEST_F(DebugApiTest, givenGetRegisterSetPropertiesCalledCorrectPropertiesReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(device->toHandle(), &count, nullptr));
    EXPECT_EQ(12u, count);

    std::vector<zet_debug_regset_properties_t> regsetProps(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zetDebugGetRegisterSetProperties(device->toHandle(), &count, regsetProps.data()));

    EXPECT_EQ(12u, count);

    auto validateRegsetProps = [](const zet_debug_regset_properties_t &regsetProps,
                                  zet_debug_regset_type_intel_gpu_t type, zet_debug_regset_flags_t flags,
                                  uint32_t num, uint32_t bits, uint32_t bytes) {
        EXPECT_EQ(regsetProps.stype, ZET_STRUCTURE_TYPE_DEBUG_REGSET_PROPERTIES);
        EXPECT_EQ(regsetProps.pNext, nullptr);
        EXPECT_EQ(regsetProps.type, type);
        EXPECT_EQ(regsetProps.version, 0u);
        EXPECT_EQ(regsetProps.generalFlags, flags);
        EXPECT_EQ(regsetProps.count, num);
        EXPECT_EQ(regsetProps.bitSize, bits);
        EXPECT_EQ(regsetProps.byteSize, bytes);
    };

    validateRegsetProps(regsetProps[0], ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 128, 256, 32);
    validateRegsetProps(regsetProps[1], ZET_DEBUG_REGSET_TYPE_ADDR_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 1, 256, 32);
    validateRegsetProps(regsetProps[2], ZET_DEBUG_REGSET_TYPE_FLAG_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 2, 32, 4);
    validateRegsetProps(regsetProps[3], ZET_DEBUG_REGSET_TYPE_CE_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE, 1, 32, 4);
    validateRegsetProps(regsetProps[4], ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 2, 128, 16);
    validateRegsetProps(regsetProps[5], ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 1, 128, 16);
    validateRegsetProps(regsetProps[6], ZET_DEBUG_REGSET_TYPE_TDR_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE, 1, 128, 16);
    validateRegsetProps(regsetProps[7], ZET_DEBUG_REGSET_TYPE_ACC_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 10, 256, 32);
    // MME is not present
    validateRegsetProps(regsetProps[8], ZET_DEBUG_REGSET_TYPE_SP_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 1, 128, 16);
    validateRegsetProps(regsetProps[9], ZET_DEBUG_REGSET_TYPE_SBA_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE, ZET_DEBUG_SBA_COUNT_INTEL_GPU, 64, 8);
    validateRegsetProps(regsetProps[10], ZET_DEBUG_REGSET_TYPE_DBG_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 1, 32, 4);
    validateRegsetProps(regsetProps[11], ZET_DEBUG_REGSET_TYPE_FC_INTEL_GPU, ZET_DEBUG_REGSET_FLAG_READABLE | ZET_DEBUG_REGSET_FLAG_WRITEABLE, 1, 32, 4);
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

TEST(DebugSessionTest, givenTileAttachDisabledAndSubDeviceWhenCreatingSessionThenNullptrReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(0);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = true;

    ze_result_t result = ZE_RESULT_ERROR_DEVICE_LOST;
    auto session = deviceImp.createDebugSession(config, result, false);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST(DebugSessionTest, givenTileAttachDisabledAndSubDeviceWhenDebugAttachCalledThenErrorReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(0);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto neoDevice = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    neoDevice->incRefInternal();
    auto neoSubdevice = std::unique_ptr<NEO::SubDevice>(neoDevice->createSubDevice(0));

    auto deviceImp = std::make_unique<Mock<L0::DeviceImp>>(neoSubdevice.get(), neoSubdevice->getExecutionEnvironment());
    deviceImp->isSubdevice = true;

    ze_result_t result = ZE_RESULT_ERROR_DEVICE_LOST;
    zet_debug_session_handle_t debugSession = nullptr;
    result = zetDebugAttach(deviceImp->toHandle(), &config, &debugSession);

    EXPECT_EQ(nullptr, debugSession);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST(DebugSessionTest, givenRootDeviceWhenCreatingSessionThenResultReturnedIsCorrect) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    NEO::Device *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));

    auto osInterface = new OsInterfaceWithDebugAttach;
    osInterface->debugAttachAvailable = false;

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.isSubdevice = false;

    ze_result_t result = ZE_RESULT_ERROR_DEVICE_LOST;
    auto session = deviceImp.createDebugSession(config, result, true);

    EXPECT_EQ(nullptr, session);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(DebugApiTest, givenZeAffinityMaskAndEnabledDebugMessagesWhenDebugAttachCalledThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.PrintDebugMessages.set(1);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_AFFINITY_MASK", "0.1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    zet_debug_config_t config = {};
    config.pid = 0x1234;

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    deviceImp.debugSession.reset(new DebugSessionMock(config, &deviceImp));

    testing::internal::CaptureStdout();
    zet_debug_session_handle_t debugSession = nullptr;
    zetDebugAttach(deviceImp.toHandle(), &config, &debugSession);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(std::string("ZE_AFFINITY_MASK is not recommended while using program debug API\n"), output);
}

} // namespace ult
} // namespace L0
