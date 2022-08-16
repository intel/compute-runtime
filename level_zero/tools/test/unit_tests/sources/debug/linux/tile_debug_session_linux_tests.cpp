/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include <memory>
namespace L0 {
namespace ult {

TEST(TileDebugSessionLinuxTest, GivenTileDebugSessionWhenCallingFunctionsThenUnsupportedErrorIsReturnedOrImplementationIsEmpty) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());

    auto session = std::make_unique<MockTileDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, nullptr);
    ASSERT_NE(nullptr, session);

    ze_device_thread_t thread = {0, 0, 0, 0};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->interrupt(thread));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->resume(thread));

    uint32_t type = 0;
    uint32_t start = 0;
    uint32_t count = 1;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readRegisters(thread, type, start, count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->writeRegisters(thread, type, start, count, nullptr));

    EuThread::ThreadId threadId{0, 0, 0, 0, 0};
    NEO::SbaTrackedAddresses sbaBuffer = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, session->readSbaBuffer(threadId, sbaBuffer));

    EXPECT_TRUE(session->closeConnection());
    EXPECT_TRUE(session->readModuleDebugArea());

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->initialize());

    EXPECT_THROW(session->startAsyncThread(), std::exception);
}

TEST(TileDebugSessionLinuxTest, GivenTileDebugSessionWhenCallingFunctionsThenCallsAreRedirectedToRootSession) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto rootSession = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    rootSession->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto session = std::make_unique<MockTileDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, rootSession.get());
    ASSERT_NE(nullptr, session);

    DebugSessionLinux::BindInfo cssaInfo = {0x1234000, 0x400};
    rootSession->clientHandleToConnection[rootSession->clientHandle]->vmToContextStateSaveAreaBindInfo[5] = cssaInfo;

    EXPECT_EQ(0x1234000u, session->getContextStateSaveAreaGpuVa(5));

    auto allVms = session->getAllMemoryHandles();
    EXPECT_EQ(0u, allVms.size());

    rootSession->clientHandleToConnection[rootSession->clientHandle]->vmIds.insert(6u);

    allVms = session->getAllMemoryHandles();
    EXPECT_EQ(1u, allVms.size());
    EXPECT_EQ(6u, allVms[0]);
}

TEST(TileDebugSessionLinuxTest, GivenTileDebugSessionWhenReadingContextStateSaveAreaHeaderThenHeaderIsCopiedFromRootSession) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    Mock<L0::DeviceImp> deviceImp(neoDevice, neoDevice->getExecutionEnvironment());
    auto rootSession = std::make_unique<MockDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    rootSession->clientHandle = MockDebugSessionLinux::mockClientHandle;

    auto session = std::make_unique<MockTileDebugSessionLinux>(zet_debug_config_t{0x1234}, &deviceImp, rootSession.get());
    ASSERT_NE(nullptr, session);

    session->readStateSaveAreaHeader();
    EXPECT_TRUE(session->stateSaveAreaHeader.empty());

    const char *header = "cssa";
    rootSession->stateSaveAreaHeader.assign(header, header + sizeof(header));

    session->readStateSaveAreaHeader();
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());

    const char *data = session->stateSaveAreaHeader.data();
    EXPECT_STREQ(header, data);
}

struct TileAttachFixture : public DebugApiLinuxMultiDeviceFixture, public MockDebugSessionLinuxHelper {
    void setUp() {
        NEO::DebugManager.flags.ExperimentalEnableTileAttach.set(1);

        DebugApiLinuxMultiDeviceFixture::setUp();

        zet_debug_config_t config = {};
        config.pid = 0x1234;
        auto session = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);
        ASSERT_NE(nullptr, session);
        session->clientHandle = MockDebugSessionLinux::mockClientHandle;
        session->createTileSessionsIfEnabled();
        rootSession = session.get();

        tileSessions[0] = reinterpret_cast<MockTileDebugSessionLinux *>(rootSession->tileSessions[0].first);
        tileSessions[1] = reinterpret_cast<MockTileDebugSessionLinux *>(rootSession->tileSessions[1].first);

        setupSessionClassHandlesAndUuidMap(session.get());
        setupVmToTile(session.get());

        deviceImp->setDebugSession(session.release());
    }

    void tearDown() {
        DebugApiLinuxMultiDeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
    MockDebugSessionLinux *rootSession = nullptr;
    MockTileDebugSessionLinux *tileSessions[2];
};

using TileAttachTest = Test<TileAttachFixture>;

TEST_F(TileAttachTest, GivenTileAttachEnabledAndMultitileDeviceWhenInitializingDebugSessionThenTileSessionsAreCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandler;
    handler->pollRetVal = 1;

    prelim_drm_i915_debug_event_client clientCreate = {};
    clientCreate.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientCreate.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    clientCreate.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientCreate.handle = MockDebugSessionLinux::mockClientHandle;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientCreate), static_cast<uint64_t>(clientCreate.base.size)});

    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinux::mockClientHandle;
    session->clientHandleToConnection[session->clientHandle]->vmToContextStateSaveAreaBindInfo[1u] = {0x1000, 0x1000};

    session->initialize();

    ASSERT_EQ(numSubDevices, session->tileSessions.size());

    EXPECT_EQ(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>(), session->tileSessions[0].first->getConnectedDevice());
    EXPECT_EQ(neoDevice->getSubDevice(1)->getSpecializedDevice<L0::Device>(), session->tileSessions[1].first->getConnectedDevice());

    EXPECT_FALSE(session->tileSessions[0].second);
    EXPECT_FALSE(session->tileSessions[1].second);
}

TEST_F(TileAttachTest, GivenTileAttachDisabledAndMultitileDeviceWhenCreatingTileSessionsThenSessionsAreNotCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinux>(config, deviceImp, 10);
    ASSERT_NE(nullptr, session);

    session->tileAttachEnabled = false;
    session->createTileSessionsIfEnabled();

    ASSERT_EQ(0u, session->tileSessions.size());
}

TEST_F(TileAttachTest, givenTileDeviceWhenCallingDebugAttachAndDetachThenSuccessAndValidSessionHandleAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession1 = nullptr;

    auto result = zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugAttach(neoDevice->getSubDevice(1)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession1);

    result = zetDebugDetach(debugSession1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetDebugDetach(debugSession0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(TileAttachTest, givenTileDeviceWhenCallingDebugAttachAndDetachManyTimesThenSuccessAndValidSessionHandleAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;
    rootSession->tileSessions[1].second = true; //prevent destroying root session

    auto result = zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugDetach(debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugDetach(debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(TileAttachTest, givenTileDeviceWhenCallingDebugAttachTwiceThenTheSameSessionIsRrturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession0Second = nullptr;

    auto result = zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    result = zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0Second);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(debugSession0Second, debugSession0);
}

TEST_F(TileAttachTest, givenCmdQsCreatedAndDestroyedWhenReadingEventsThenProcessEntryAndExitAreReturned) {

    prelim_drm_i915_debug_event_uuid uuid = {};
    uuid.base.type = PRELIM_DRM_I915_DEBUG_EVENT_UUID;
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    uuid.base.size = sizeof(prelim_drm_i915_debug_event_uuid);
    uuid.client_handle = MockDebugSessionLinux::mockClientHandle;
    uuid.handle = 2;
    uuid.payload_size = sizeof(NEO::DebuggerL0::CommandQueueNotification);

    auto uuidHash = NEO::uuidL0CommandQueueHash;
    prelim_drm_i915_debug_read_uuid readUuid = {};
    memcpy(readUuid.uuid, uuidHash, strlen(uuidHash));

    NEO::DebuggerL0::CommandQueueNotification notification;
    notification.subDeviceCount = 2;
    notification.subDeviceIndex = 1;
    readUuid.payload_ptr = reinterpret_cast<uint64_t>(&notification);
    readUuid.payload_size = sizeof(NEO::DebuggerL0::CommandQueueNotification);
    readUuid.handle = uuid.handle;

    auto handler = new MockIoctlHandler;
    rootSession->ioctlHandler.reset(handler);
    handler->returnUuid = &readUuid;

    // Handle UUID create for commandQueue on subdevice 1
    rootSession->handleEvent(&uuid.base);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);

    notification.subDeviceCount = 2;
    notification.subDeviceIndex = 0;
    uuid.handle = 3;
    handler->returnUuid = &readUuid;

    // Handle UUID create for commandQueue on subdevice 0
    rootSession->handleEvent(&uuid.base);

    event = {};
    result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);

    // Handle UUID destroy for commandQueue on subdevice 0
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    rootSession->handleEvent(&uuid.base);

    event = {};
    result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event.type);

    // Handle UUID destroy for commandQueue on subdevice 1
    uuid.handle = 2;
    rootSession->handleEvent(&uuid.base);

    event = {};
    result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event.type);
}

TEST_F(TileAttachTest, givenPollReturnsErrorAndEinvalWhenReadingEventsThenProcessDetachedEventForAllTilesIsReturned) {

    auto handler = new MockIoctlHandler;
    rootSession->ioctlHandler.reset(handler);
    handler->pollRetVal = -1;
    errno = EINVAL;

    rootSession->readInternalEventsAsync();

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_DETACHED, event.type);

    result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_DETACHED, event.type);
}

TEST_F(TileAttachTest, GivenTileAndVmBindForIsaWithAckWhenReadingEventThenModuleLoadWithAckIsReturnedForAttachedTile) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    addIsaVmBindEvent(rootSession, vm0, true, true);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(ZET_DEBUG_EVENT_FLAG_NEED_ACK, event.flags);

    EXPECT_EQ(isaGpuVa, event.info.module.load);

    auto elf = rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr;
    auto elfSize = rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize;
    EXPECT_EQ(elf, event.info.module.moduleBegin);
    EXPECT_EQ(elf + elfSize, event.info.module.moduleEnd);

    // Handle event for tile 1
    addIsaVmBindEvent(rootSession, vm1, true, true);

    zetDebugAttach(neoDevice->getSubDevice(1)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);
    event = {};
    result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(0u, event.flags);

    EXPECT_EQ(isaGpuVa, event.info.module.load);
    EXPECT_EQ(elf, event.info.module.moduleBegin);
    EXPECT_EQ(elf + elfSize, event.info.module.moduleEnd);
}

TEST_F(TileAttachTest, GivenTileAndVmBindForIsaWithoutAckWhenReadingEventThenModuleLoadIsReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    addIsaVmBindEvent(rootSession, vm0, false, true);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(0u, event.flags);
    EXPECT_EQ(isaGpuVa, event.info.module.load);

    // Handle event for tile 1
    addIsaVmBindEvent(rootSession, vm1, false, true);

    zetDebugAttach(neoDevice->getSubDevice(1)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);
    event = {};
    result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
    EXPECT_EQ(0u, event.flags);
    EXPECT_EQ(isaGpuVa, event.info.module.load);
}

TEST_F(TileAttachTest, GivenTileAndVmBindEventsForIsaWhenReadingEventThenModuleLoadAndUnloadEventsAreReturned) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    addIsaVmBindEvent(rootSession, vm0, true, true);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);

    addIsaVmBindEvent(rootSession, vm0, false, false);

    event = {};
    result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
    EXPECT_EQ(0u, event.flags);
}

TEST_F(TileAttachTest, GivenIsaWhenReadingOrWritingMemoryThenMemoryIsReadAndWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    addIsaVmBindEvent(rootSession, vm0, true, true);

    auto handler = new MockIoctlHandler;
    rootSession->ioctlHandler.reset(handler);

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

    zet_debug_memory_space_desc_t desc;
    desc.address = isaGpuVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[bufferSize];
    memset(output, 0, bufferSize);

    handler->preadRetVal = bufferSize;
    ze_result_t result = zetDebugReadMemory(tileSessions[0]->toHandle(), thread, &desc, bufferSize, &output);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }

    char writeBuffer[bufferSize];
    handler->setPwriteMemory(writeBuffer, bufferSize, desc.address);

    result = zetDebugWriteMemory(tileSessions[0]->toHandle(), thread, &desc, bufferSize, &output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, handler->pwriteCalled);
}

TEST_F(TileAttachTest, GivenElfAddressWhenReadMemoryCalledTheElfMemoryIsRead) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    auto handler = new MockIoctlHandler;
    rootSession->ioctlHandler.reset(handler);
    rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->elfMap[elfVa] = elfUUID;

    ze_device_thread_t thread = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

    zet_debug_memory_space_desc_t desc;
    desc.address = elfVa;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    char output[16];
    memset(output, 0, sizeof(output));

    ze_result_t result = zetDebugReadMemory(tileSessions[0]->toHandle(), thread, &desc, elfSize, &output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_STREQ("ELF", output);
}

TEST_F(TileAttachTest, WhenCallingReadWriteMemoryforASingleThreadThenMemoryIsReadAndWritten) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    addIsaVmBindEvent(rootSession, vm0, true, true);

    auto handler = new MockIoctlHandler;
    rootSession->ioctlHandler.reset(handler);

    zet_debug_memory_space_desc_t desc;
    desc.address = 0x12345000;
    desc.type = ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT;

    ze_device_thread_t thread = {0, 0, 0, 0};

    char output[bufferSize];
    tileSessions[0]->ensureThreadStopped(thread, vm0);

    handler->preadRetVal = bufferSize;
    auto result = zetDebugReadMemory(tileSessions[0]->toHandle(), thread, &desc, bufferSize, &output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }

    char writeBuffer[bufferSize];
    desc.address = reinterpret_cast<uint64_t>(writeBuffer);
    handler->setPwriteMemory(writeBuffer, bufferSize, desc.address);

    result = zetDebugWriteMemory(tileSessions[0]->toHandle(), thread, &desc, bufferSize, &output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, handler->pwriteCalled);
}

TEST_F(TileAttachTest, GivenEventWithL0ZebinModuleWhenHandlingEventThenModuleLoadAndUnloadEventsAreReportedForLastKernel) {
    uint64_t isaGpuVa = 0x345000;
    uint64_t isaGpuVa2 = 0x340000;
    uint64_t isaSize = 0x2000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    rootSession->tileSessions[0].second = true;
    auto handler = new MockIoctlHandler;
    rootSession->ioctlHandler.reset(handler);

    for (uint32_t tile = 0; tile < 2; tile++) {
        auto handler = new MockIoctlHandler;
        rootSession->ioctlHandler.reset(handler);

        uint64_t vmHandle = 0;
        switch (tile) {
        case 0:
            vmHandle = vm0;
            break;
        case 1:
            vmHandle = vm1;
            break;
        }

        vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
        vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
        vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
        vmBindIsa->base.seqno = 10;
        vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
        vmBindIsa->va_start = isaGpuVa;
        vmBindIsa->va_length = isaSize;
        vmBindIsa->vm_handle = vmHandle;
        vmBindIsa->num_uuids = 4;
        auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
        typeOfUUID uuidsTemp[4];
        uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
        uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
        uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
        uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleUUID);

        memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));
        rootSession->handleEvent(&vmBindIsa->base);

        EXPECT_EQ(0u, rootSession->apiEvents.size());
        EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
        EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[tile].size());

        // event not pushed to ack
        EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[tile][isaGpuVa]->ackEvents.size());
        EXPECT_EQ(1, handler->ioctlCalled); // ACK
        EXPECT_EQ(vmBindIsa->base.seqno, handler->debugEventAcked.seqno);

        vmBindIsa->va_start = isaGpuVa2;
        vmBindIsa->base.seqno = 11;
        handler->ioctlCalled = 0;
        handler->debugEventAcked.seqno = 0;

        rootSession->handleEvent(&vmBindIsa->base);

        EXPECT_EQ(0u, rootSession->apiEvents.size());
        EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule.size());
        EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[tile].size());
        EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount);

        auto &isaMap = rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[tile];
        EXPECT_EQ(2u, isaMap.size());

        bool attachedAfterModuleLoaded = false;
        if (rootSession->tileSessions[tile].second == false) {
            rootSession->tileSessions[tile].second = true;
            attachedAfterModuleLoaded = true;
        }

        if (attachedAfterModuleLoaded == false) {
            // event not pushed to ack
            EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[tile][isaGpuVa2]->ackEvents.size());
            EXPECT_EQ(0, handler->ioctlCalled);
            EXPECT_EQ(0u, handler->debugEventAcked.seqno);

            EXPECT_FALSE(isaMap[isaGpuVa]->moduleLoadEventAck);
            EXPECT_FALSE(isaMap[isaGpuVa2]->moduleLoadEventAck);
        } else {

            // event not pushed to ack
            EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->isaMap[tile][isaGpuVa2]->ackEvents.size());
            // ack immediately
            EXPECT_EQ(1, handler->ioctlCalled);
            EXPECT_EQ(11u, handler->debugEventAcked.seqno);
            handler->ioctlCalled = 0;
            handler->debugEventAcked.seqno = 0;

            EXPECT_TRUE(isaMap[isaGpuVa]->moduleLoadEventAck);
            EXPECT_TRUE(isaMap[isaGpuVa2]->moduleLoadEventAck);
        }

        zet_debug_event_t event = {};
        ze_result_t result = zetDebugReadEvent(tileSessions[tile]->toHandle(), 0, &event);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_LOAD, event.type);
        if (attachedAfterModuleLoaded == false) {
            EXPECT_EQ(ZET_DEBUG_EVENT_FLAG_NEED_ACK, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);
        } else {
            EXPECT_EQ(0u, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);
        }

        EXPECT_EQ(isaGpuVa2, event.info.module.load);

        auto elfAddress = rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr;
        auto elfSize = rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize;
        EXPECT_EQ(elfAddress, event.info.module.moduleBegin);
        EXPECT_EQ(elfAddress + elfSize, event.info.module.moduleEnd);

        result = zetDebugAcknowledgeEvent(tileSessions[tile]->toHandle(), &event);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        if (event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK) {
            // vm_bind acked
            EXPECT_EQ(1, handler->ioctlCalled);
            EXPECT_EQ(11u, handler->debugEventAcked.seqno);
        }

        vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        vmBindIsa->va_start = isaGpuVa2;

        rootSession->handleEvent(&vmBindIsa->base);

        EXPECT_EQ(0u, rootSession->apiEvents.size());

        vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        vmBindIsa->va_start = isaGpuVa;

        rootSession->handleEvent(&vmBindIsa->base);

        EXPECT_EQ(0u, rootSession->apiEvents.size());

        memset(&event, 0, sizeof(zet_debug_event_t));

        result = zetDebugReadEvent(tileSessions[tile]->toHandle(), 0, &event);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD, event.type);
        EXPECT_EQ(isaGpuVa2, event.info.module.load);
        EXPECT_EQ(0u, event.flags & ZET_DEBUG_EVENT_FLAG_NEED_ACK);

        EXPECT_EQ(rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr, event.info.module.moduleBegin);
        EXPECT_EQ(rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr +
                      rootSession->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].dataSize,
                  event.info.module.moduleEnd);
    }
}

} // namespace ult
} // namespace L0
