/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"
#include "level_zero/tools/test/unit_tests/sources/debug/linux/prelim/debug_session_fixtures_linux.h"
#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"

#include <memory>

namespace NEO {
namespace SysCalls {
extern uint32_t closeFuncCalled;
extern int closeFuncArgPassed;
extern int closeFuncRetVal;
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace ult {

TEST(TileDebugSessionLinuxi915Test, GivenTileDebugSessionWhenCallingFunctionsThenImplementationIsEmpty) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    MockDeviceImp deviceImp(neoDevice);

    auto session = std::make_unique<MockTileDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, nullptr);
    ASSERT_NE(nullptr, session);

    EXPECT_TRUE(session->closeConnection());
    EXPECT_TRUE(session->readModuleDebugArea());

    EXPECT_EQ(ZE_RESULT_SUCCESS, session->initialize());

    EXPECT_THROW(session->startAsyncThread(), std::exception);
    EXPECT_THROW(session->cleanRootSessionAfterDetach(0), std::exception);
}

TEST(TileDebugSessionLinuxi915Test, GivenTileDebugSessionWhenCallingFunctionsThenCallsAreRedirectedToRootSession) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    MockDeviceImp deviceImp(neoDevice);
    auto rootSession = std::make_unique<MockDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    rootSession->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto session = std::make_unique<MockTileDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, rootSession.get());
    ASSERT_NE(nullptr, session);

    DebugSessionLinuxi915::BindInfo cssaInfo = {0x1234000, 0x400};
    rootSession->clientHandleToConnection[rootSession->clientHandle]->vmToContextStateSaveAreaBindInfo[5] = cssaInfo;

    EXPECT_EQ(0x1234000u, session->getContextStateSaveAreaGpuVa(5));
    EXPECT_EQ(0x400u, session->getContextStateSaveAreaSize(5));

    auto allVms = session->getAllMemoryHandles();
    EXPECT_EQ(0u, allVms.size());

    rootSession->clientHandleToConnection[rootSession->clientHandle]->vmIds.insert(6u);

    allVms = session->getAllMemoryHandles();
    EXPECT_EQ(1u, allVms.size());
    EXPECT_EQ(6u, allVms[0]);

    auto sbaGpuVa = session->getSbaBufferGpuVa(5);
    EXPECT_EQ(0u, sbaGpuVa);

    DebugSessionLinuxi915::BindInfo sbaInfo = {0x567000, 0x200};
    rootSession->clientHandleToConnection[rootSession->clientHandle]->vmToStateBaseAreaBindInfo[5] = sbaInfo;

    sbaGpuVa = session->getSbaBufferGpuVa(5);
    auto rootSbaGpuVa = rootSession->getSbaBufferGpuVa(5);
    EXPECT_EQ(0x567000u, sbaGpuVa);
    EXPECT_EQ(rootSbaGpuVa, sbaGpuVa);

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    session->ioctl(0, nullptr);
    EXPECT_EQ(1, handler->ioctlCalled);
}

TEST(TileDebugSessionLinuxi915Test, GivenTileDebugSessionWhenReadingContextStateSaveAreaHeaderThenHeaderIsCopiedFromRootSession) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    MockDeviceImp deviceImp(neoDevice);
    auto rootSession = std::make_unique<MockDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    rootSession->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto session = std::make_unique<MockTileDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, rootSession.get());
    ASSERT_NE(nullptr, session);

    session->readStateSaveAreaHeader();
    EXPECT_TRUE(session->stateSaveAreaHeader.empty());

    const char *header = "cssa";
    rootSession->stateSaveAreaHeader.assign(header, header + strlen(header) + 1);

    session->readStateSaveAreaHeader();
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());

    const char *data = session->stateSaveAreaHeader.data();
    EXPECT_STREQ(header, data);
}

TEST(TileDebugSessionLinuxi915Test, GivenTileDebugSessionWhenReadingContextStateSaveAreaHeaderThenSlmSupportIsSetFromRootSession) {
    auto hwInfo = *NEO::defaultHwInfo.get();
    NEO::MockDevice *neoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);

    MockDeviceImp deviceImp(neoDevice);
    auto rootSession = std::make_unique<MockDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, 10);
    rootSession->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;

    auto session = std::make_unique<MockTileDebugSessionLinuxi915>(zet_debug_config_t{0x1234}, &deviceImp, rootSession.get());
    ASSERT_NE(nullptr, session);

    const char *header = "cssa";
    rootSession->stateSaveAreaHeader.assign(header, header + strlen(header) + 1);
    rootSession->sipSupportsSlm = false;

    session->readStateSaveAreaHeader();
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    EXPECT_FALSE(session->sipSupportsSlm);

    rootSession->sipSupportsSlm = true;
    session->readStateSaveAreaHeader();
    EXPECT_FALSE(session->stateSaveAreaHeader.empty());
    EXPECT_TRUE(session->sipSupportsSlm);
}

template <bool blockOnFence = false>
struct TileAttachFixture : public DebugApiLinuxMultiDeviceFixture, public MockDebugSessionLinuxi915Helper {
    void setUp() {
        NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

        DebugApiLinuxMultiDeviceFixture::setUp();

        zet_debug_config_t config = {};
        config.pid = 0x1234;
        auto session = std::make_unique<MockDebugSessionLinuxi915>(config, deviceImp, 10);
        ASSERT_NE(nullptr, session);
        session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;
        session->createTileSessionsIfEnabled();
        rootSession = session.get();
        rootSession->blockOnFenceMode = blockOnFence;

        tileSessions[0] = static_cast<MockTileDebugSessionLinuxi915 *>(rootSession->tileSessions[0].first);
        tileSessions[1] = static_cast<MockTileDebugSessionLinuxi915 *>(rootSession->tileSessions[1].first);

        setupSessionClassHandlesAndUuidMap(session.get());
        setupVmToTile(session.get());

        deviceImp->setDebugSession(session.release());
    }

    void tearDown() {
        DebugApiLinuxMultiDeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
    MockDebugSessionLinuxi915 *rootSession = nullptr;
    MockTileDebugSessionLinuxi915 *tileSessions[2];
};

using TileAttachTest = Test<TileAttachFixture<>>;

TEST_F(TileAttachTest, GivenTileAttachEnabledAndMultitileDeviceWhenInitializingDebugSessionThenTileSessionsAreCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxi915>(config, deviceImp, 10);
    ASSERT_NE(nullptr, session);

    auto handler = new MockIoctlHandlerI915;
    handler->pollRetVal = 1;

    prelim_drm_i915_debug_event_client clientCreate = {};
    clientCreate.base.type = PRELIM_DRM_I915_DEBUG_EVENT_CLIENT;
    clientCreate.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
    clientCreate.base.size = sizeof(prelim_drm_i915_debug_event_client);
    clientCreate.handle = MockDebugSessionLinuxi915::mockClientHandle;
    handler->eventQueue.push({reinterpret_cast<char *>(&clientCreate), static_cast<uint64_t>(clientCreate.base.size)});

    session->ioctlHandler.reset(handler);
    session->clientHandle = MockDebugSessionLinuxi915::mockClientHandle;
    session->clientHandleToConnection[session->clientHandle]->vmToContextStateSaveAreaBindInfo[1u] = {0x1000, 0x1000};
    session->synchronousInternalEventRead = true;
    session->initialize();

    ASSERT_EQ(numSubDevices, session->tileSessions.size());

    EXPECT_EQ(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>(), session->tileSessions[0].first->getConnectedDevice());
    EXPECT_EQ(neoDevice->getSubDevice(1)->getSpecializedDevice<L0::Device>(), session->tileSessions[1].first->getConnectedDevice());

    EXPECT_FALSE(session->tileSessions[0].second);
    EXPECT_FALSE(session->tileSessions[1].second);

    auto threadId0 = tileSessions[0]->allThreads.begin()->second->getThreadId();
    auto threadId1 = tileSessions[1]->allThreads.begin()->second->getThreadId();

    EXPECT_EQ(0u, threadId0.tileIndex);
    EXPECT_EQ(1u, threadId1.tileIndex);

    threadId0 = tileSessions[0]->allThreads.rbegin()->second->getThreadId();
    threadId1 = tileSessions[1]->allThreads.rbegin()->second->getThreadId();

    EXPECT_EQ(0u, threadId0.tileIndex);
    EXPECT_EQ(1u, threadId1.tileIndex);
}

TEST_F(TileAttachTest, GivenTileAttachDisabledAndMultitileDeviceWhenCreatingTileSessionsThenSessionsAreNotCreated) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;

    auto session = std::make_unique<MockDebugSessionLinuxi915>(config, deviceImp, 10);
    ASSERT_NE(nullptr, session);

    session->tileAttachEnabled = false;
    session->createTileSessionsIfEnabled();

    ASSERT_EQ(0u, session->tileSessions.size());
}

TEST_F(TileAttachTest, givenTileDeviceWhenCallingDebugDetachOnLastSessionThenRootSessionIsClosed) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    auto result = zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, debugSession0);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;

    auto debugFd = rootSession->fd;
    result = zetDebugDetach(debugSession0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(debugFd, NEO::SysCalls::closeFuncArgPassed);

    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
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
    rootSession->tileSessions[1].second = true; // prevent destroying root session

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

TEST_F(TileAttachTest, givenTileDeviceWhenCallingDebugAttachTwiceThenTheSameSessionIsReturned) {
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
    uuid.client_handle = MockDebugSessionLinuxi915::mockClientHandle;
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

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);
    handler->returnUuid = &readUuid;

    // Handle UUID create for commandQueue on subdevice 1
    rootSession->handleEvent(&uuid.base);

    zet_debug_event_t event = {};
    ze_result_t result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);
    EXPECT_TRUE(tileSessions[1]->processEntryState);

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
    EXPECT_TRUE(tileSessions[0]->processEntryState);

    // Handle UUID destroy for commandQueue on subdevice 0
    uuid.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    rootSession->handleEvent(&uuid.base);

    event = {};
    result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event.type);
    EXPECT_FALSE(tileSessions[0]->processEntryState);

    // Handle UUID destroy for commandQueue on subdevice 1
    uuid.handle = 2;
    rootSession->handleEvent(&uuid.base);

    event = {};
    result = zetDebugReadEvent(tileSessions[1]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT, event.type);
    EXPECT_FALSE(tileSessions[1]->processEntryState);
}

TEST_F(TileAttachTest, givenTileSessionWhenAttchingThenProcessEntryEventIsGeneratedBasedOnEntryState) {

    tileSessions[1]->processEntryState = true;

    tileSessions[1]->attachTile();
    ASSERT_EQ(1u, tileSessions[1]->apiEvents.size());

    auto event = tileSessions[1]->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY, event.type);

    tileSessions[1]->detachTile();
    tileSessions[1]->processEntryState = false;

    tileSessions[1]->attachTile();
    EXPECT_EQ(0u, tileSessions[1]->apiEvents.size());
}

TEST_F(TileAttachTest, givenDetachedRootSessionWhenAttchingTileThenDetachedEventIsGenerated) {
    tileSessions[1]->detached = true;

    tileSessions[1]->attachTile();
    ASSERT_EQ(1u, tileSessions[1]->apiEvents.size());

    auto event = tileSessions[1]->apiEvents.front();
    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_DETACHED, event.type);
}

TEST_F(TileAttachTest, givenPollReturnsErrorAndEinvalWhenReadingEventsThenProcessDetachedEventForAllTilesIsReturned) {

    auto handler = new MockIoctlHandlerI915;
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

    auto elf = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].ptr;
    auto elfSize = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].dataSize;
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
    zet_debug_session_handle_t debugSession0 = nullptr, debugSession1 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    addIsaVmBindEvent(rootSession, vm0, true, true);

    auto handler = new MockIoctlHandlerI915;
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
    EXPECT_EQ(vm0, handler->vmOpen.handle);

    for (int i = 0; i < bufferSize; i++) {
        EXPECT_EQ(static_cast<char>(0xaa), output[i]);
    }

    char writeBuffer[bufferSize];
    handler->setPwriteMemory(writeBuffer, bufferSize, desc.address);

    result = zetDebugWriteMemory(tileSessions[0]->toHandle(), thread, &desc, bufferSize, &output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, handler->pwriteCalled);
    EXPECT_EQ(vm0, handler->vmOpen.handle);

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession1);
    addIsaVmBindEvent(rootSession, vm1, true, true);
    handler->preadCalled = 0;

    result = zetDebugReadMemory(tileSessions[1]->toHandle(), thread, &desc, bufferSize, &output);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(vm1, handler->vmOpen.handle);
    EXPECT_EQ(1u, handler->preadCalled);
}

TEST_F(TileAttachTest, GivenElfAddressWhenReadMemoryCalledTheElfMemoryIsRead) {
    zet_debug_config_t config = {};
    config.pid = 0x1234;
    zet_debug_session_handle_t debugSession0 = nullptr;

    zetDebugAttach(neoDevice->getSubDevice(0)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &debugSession0);

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->elfMap[elfVa] = elfUUID;

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

    auto handler = new MockIoctlHandlerI915;
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

TEST_F(TileAttachTest, givenExecutingThreadWhenInterruptingAndResumingThenCallsAreSentThroughRootSession) {
    // deubg attach both tiles
    rootSession->tileSessions[0].second = true;
    rootSession->tileSessions[1].second = true;

    SIP::version version = {2, 0, 0};
    initStateSaveArea(rootSession->stateSaveAreaHeader, version, deviceImp);

    ze_device_thread_t apiThread = {0, 0, 0, 0};

    for (uint32_t tile = 0; tile < 2; tile++) {
        EuThread::ThreadId threadId = {tile, apiThread};

        auto result = zetDebugInterrupt(tileSessions[tile]->toHandle(), apiThread);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        tileSessions[tile]->sendInterrupts();
        EXPECT_EQ(tile, rootSession->interruptedDevice);

        tileSessions[tile]->ensureThreadStopped(apiThread, 4);
        tileSessions[tile]->writeResumeResult = 1;

        result = zetDebugResume(tileSessions[tile]->toHandle(), apiThread);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        ASSERT_EQ(1u, rootSession->resumedThreads.size());
        ASSERT_EQ(1u, rootSession->resumedDevices.size());
        EXPECT_EQ(threadId.slice, rootSession->resumedThreads[0][0].slice);
        EXPECT_EQ(threadId.subslice, rootSession->resumedThreads[0][0].subslice);
        EXPECT_EQ(threadId.eu, rootSession->resumedThreads[0][0].eu);
        EXPECT_EQ(threadId.thread, rootSession->resumedThreads[0][0].thread);
        EXPECT_EQ(threadId.tileIndex, rootSession->resumedThreads[0][0].tileIndex);
        EXPECT_EQ(tile, rootSession->resumedDevices[0]);

        rootSession->resumedThreads.clear();
        rootSession->resumedDevices.clear();
    }
}

TEST_F(TileAttachTest, givenTwoInterruptsSentWhenCheckingTriggerEventsThenTriggerEventsIsSetForTiles) {
    // deubg attach both tiles
    rootSession->tileSessions[0].second = true;
    rootSession->tileSessions[1].second = true;

    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = zetDebugInterrupt(tileSessions[0]->toHandle(), apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    tileSessions[0]->sendInterrupts();
    EXPECT_EQ(0u, tileSessions[0]->expectedAttentionEvents);
    EXPECT_EQ(0u, rootSession->interruptedDevice);

    rootSession->newAttentionRaised();

    tileSessions[0]->checkTriggerEventsForAttention();
    EXPECT_TRUE(tileSessions[0]->triggerEvents);

    result = zetDebugInterrupt(tileSessions[1]->toHandle(), apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, tileSessions[1]->expectedAttentionEvents);

    tileSessions[1]->sendInterrupts();
    EXPECT_EQ(0u, tileSessions[1]->expectedAttentionEvents);
    EXPECT_FALSE(tileSessions[1]->triggerEvents);
    EXPECT_EQ(1u, rootSession->interruptedDevice);

    rootSession->newAttentionRaised();
    EXPECT_EQ(0u, tileSessions[1]->expectedAttentionEvents);
    tileSessions[1]->checkTriggerEventsForAttention();

    EXPECT_TRUE(tileSessions[1]->triggerEvents);
}

TEST_F(TileAttachTest, givenInterruptSentWhenHandlingAttentionEventThenTriggerEventsIsSetForTileSession) {
    // deubg attach both tiles
    rootSession->tileSessions[0].second = true;
    rootSession->tileSessions[1].second = true;

    ze_device_thread_t apiThread = {0, 0, 0, 0};

    auto result = zetDebugInterrupt(tileSessions[1]->toHandle(), apiThread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    tileSessions[1]->sendInterrupts();

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vmHandle] = 1;

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];

    tileSessions[1]->ensureThreadStopped(apiThread, vmHandle);

    auto engineInfo = mockDrm->getEngineInfo();
    auto engineInstance = engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType);

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinuxi915::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = engineInstance->engineClass;
    attention.ci.engine_instance = engineInstance->engineInstance;
    attention.bitmask_size = 0;

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    rootSession->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    EXPECT_TRUE(tileSessions[1]->triggerEvents);
}

TEST_F(TileAttachTest, givenStoppedThreadsWhenHandlingAttentionEventThenStoppedThreadsFromRaisedAttentionAreProcessed) {
    // debug attach both tiles
    rootSession->tileSessions[0].second = true;
    rootSession->tileSessions[1].second = true;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vmHandle] = 1;

    SIP::version version = {2, 0, 0};
    initStateSaveArea(rootSession->stateSaveAreaHeader, version, deviceImp);
    DebugSessionLinuxi915::BindInfo cssaInfo = {reinterpret_cast<uint64_t>(rootSession->stateSaveAreaHeader.data()), rootSession->stateSaveAreaHeader.size()};
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = cssaInfo;

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);
    handler->setPreadMemory(rootSession->stateSaveAreaHeader.data(), rootSession->stateSaveAreaHeader.size(), reinterpret_cast<uint64_t>(rootSession->stateSaveAreaHeader.data()));

    uint8_t data[sizeof(prelim_drm_i915_debug_event_eu_attention) + 128];

    auto engineInfo = mockDrm->getEngineInfo();
    auto engineInstance = engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType);

    EuThread::ThreadId thread = {1, 0, 0, 0, 0};
    tileSessions[1]->stoppedThreads[thread.packed] = 1;

    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({thread}, hwInfo, bitmask, bitmaskSize);

    prelim_drm_i915_debug_event_eu_attention attention = {};
    attention.base.type = PRELIM_DRM_I915_DEBUG_EVENT_EU_ATTENTION;
    attention.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    attention.base.size = sizeof(prelim_drm_i915_debug_event_eu_attention);
    attention.base.seqno = 2;
    attention.client_handle = MockDebugSessionLinuxi915::mockClientHandle;
    attention.lrc_handle = lrcHandle;
    attention.flags = 0;
    attention.ci.engine_class = engineInstance->engineClass;
    attention.ci.engine_instance = engineInstance->engineInstance;
    attention.bitmask_size = static_cast<uint32_t>(bitmaskSize);

    memcpy(data, &attention, sizeof(prelim_drm_i915_debug_event_eu_attention));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_eu_attention, bitmask)), bitmask.get(), std::min(size_t(128), bitmaskSize));

    rootSession->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    auto expectedThreadsToCheck = hwInfo.capabilityTable.fusedEuEnabled ? 2u : 1u;
    EXPECT_EQ(expectedThreadsToCheck, tileSessions[1]->newlyStoppedThreads.size());
    EXPECT_TRUE(tileSessions[1]->triggerEvents);
}

TEST_F(TileAttachTest, GivenNoPageFaultingThreadWhenHandlingPageFaultEventThenL0ApiEventGenerated) {

    // debug attach both tiles
    rootSession->tileSessions[0].second = true;
    rootSession->tileSessions[1].second = true;
    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vmHandle] = 1;

    SIP::version version = {2, 0, 0};
    initStateSaveArea(rootSession->stateSaveAreaHeader, version, deviceImp);
    DebugSessionLinuxi915::BindInfo cssaInfo = {reinterpret_cast<uint64_t>(rootSession->stateSaveAreaHeader.data()), rootSession->stateSaveAreaHeader.size()};
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = cssaInfo;

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);
    handler->setPreadMemory(rootSession->stateSaveAreaHeader.data(), rootSession->stateSaveAreaHeader.size(), reinterpret_cast<uint64_t>(rootSession->stateSaveAreaHeader.data()));

    uint8_t data[sizeof(prelim_drm_i915_debug_event_page_fault) + 128 * 3];

    auto engineInfo = mockDrm->getEngineInfo();
    auto engineInstance = engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType);

    EuThread::ThreadId thread = {1, 0, 0, 0, 0};
    tileSessions[1]->stoppedThreads[thread.packed] = 1;

    std::unique_ptr<uint8_t[]> bitmaskBefore, bitmaskAfter, bitmaskResolved;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({}, hwInfo, bitmaskBefore, bitmaskSize);
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({thread}, hwInfo, bitmaskAfter, bitmaskSize);
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({thread}, hwInfo, bitmaskResolved, bitmaskSize);

    prelim_drm_i915_debug_event_page_fault pf = {};
    pf.base.type = PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT;
    pf.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    pf.base.size = sizeof(prelim_drm_i915_debug_event_page_fault);
    pf.base.seqno = 2;
    pf.client_handle = MockDebugSessionLinuxi915::mockClientHandle;
    pf.lrc_handle = lrcHandle;
    pf.flags = 0;
    pf.ci.engine_class = engineInstance->engineClass;
    pf.ci.engine_instance = engineInstance->engineInstance;
    pf.bitmask_size = static_cast<uint32_t>(bitmaskSize * 3u);

    bitmaskSize = std::min(size_t(128), bitmaskSize);
    memcpy(data, &pf, sizeof(prelim_drm_i915_debug_event_page_fault));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask)), bitmaskBefore.get(), bitmaskSize);
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask) + bitmaskSize), bitmaskAfter.get(), bitmaskSize);
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask) + (2 * bitmaskSize)), bitmaskResolved.get(), bitmaskSize);
    rootSession->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));

    ASSERT_EQ(1u, tileSessions[1]->apiEvents.size());
    auto event = tileSessions[1]->apiEvents.front();
    ASSERT_EQ(event.type, ZET_DEBUG_EVENT_TYPE_PAGE_FAULT);
}

TEST_F(TileAttachTest, givenStoppedThreadsWhenHandlingPageFaultEventThenStoppedThreadsFromEventAreProcessed) {
    // debug attach both tiles
    rootSession->tileSessions[0].second = true;
    rootSession->tileSessions[1].second = true;

    uint64_t ctxHandle = 2;
    uint64_t vmHandle = 7;
    uint64_t lrcHandle = 8;

    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->contextsCreated[ctxHandle].vm = vmHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->lrcToContextHandle[lrcHandle] = ctxHandle;
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vmHandle] = 1;

    SIP::version version = {2, 0, 0};
    initStateSaveArea(rootSession->stateSaveAreaHeader, version, deviceImp);
    DebugSessionLinuxi915::BindInfo cssaInfo = {reinterpret_cast<uint64_t>(rootSession->stateSaveAreaHeader.data()), rootSession->stateSaveAreaHeader.size()};
    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToContextStateSaveAreaBindInfo[vmHandle] = cssaInfo;

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);
    handler->setPreadMemory(rootSession->stateSaveAreaHeader.data(), rootSession->stateSaveAreaHeader.size(), reinterpret_cast<uint64_t>(rootSession->stateSaveAreaHeader.data()));

    uint8_t data[sizeof(prelim_drm_i915_debug_event_page_fault) + 128 * 3];

    auto engineInfo = mockDrm->getEngineInfo();
    auto engineInstance = engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType);

    EuThread::ThreadId thread = {1, 0, 0, 0, 0};
    tileSessions[1]->stoppedThreads[thread.packed] = 1;

    std::unique_ptr<uint8_t[]> bitmaskBefore, bitmaskAfter, bitmaskResolved;
    size_t bitmaskSize = 0;
    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({}, hwInfo, bitmaskBefore, bitmaskSize);
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({}, hwInfo, bitmaskAfter, bitmaskSize);
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads({thread}, hwInfo, bitmaskResolved, bitmaskSize);

    prelim_drm_i915_debug_event_page_fault pf = {};
    pf.base.type = PRELIM_DRM_I915_DEBUG_EVENT_PAGE_FAULT;
    pf.base.flags = PRELIM_DRM_I915_DEBUG_EVENT_STATE_CHANGE;
    pf.base.size = sizeof(prelim_drm_i915_debug_event_page_fault);
    pf.base.seqno = 2;
    pf.client_handle = MockDebugSessionLinuxi915::mockClientHandle;
    pf.lrc_handle = lrcHandle;
    pf.flags = 0;
    pf.ci.engine_class = engineInstance->engineClass;
    pf.ci.engine_instance = engineInstance->engineInstance;
    pf.bitmask_size = static_cast<uint32_t>(bitmaskSize * 3u);

    bitmaskSize = std::min(size_t(128), bitmaskSize);
    memcpy(data, &pf, sizeof(prelim_drm_i915_debug_event_page_fault));
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask)), bitmaskBefore.get(), bitmaskSize);
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask) + bitmaskSize), bitmaskAfter.get(), bitmaskSize);
    memcpy(ptrOffset(data, offsetof(prelim_drm_i915_debug_event_page_fault, bitmask) + (2 * bitmaskSize)), bitmaskResolved.get(), bitmaskSize);

    rootSession->handleEvent(reinterpret_cast<prelim_drm_i915_debug_event *>(data));
    auto expectedThreadsToCheck = hwInfo.capabilityTable.fusedEuEnabled ? 2u : 1u;
    EXPECT_EQ(expectedThreadsToCheck, tileSessions[1]->newlyStoppedThreads.size());
    EXPECT_TRUE(tileSessions[1]->triggerEvents);
    EXPECT_TRUE(tileSessions[1]->allThreads[thread]->getPageFault());
}

TEST_F(TileAttachTest, GivenBlockingOnCpuDetachedTileAndZebinModulesWithEventsToAckWhenDetachingTileThenNoAckIoctlIsCalled) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_FALSE(rootSession->blockOnFenceMode);

    addZebinVmBindEvent(rootSession, vm0, true, true, 0);
    addZebinVmBindEvent(rootSession, vm0, true, true, 1);
    addZebinVmBindEvent(rootSession, vm1, true, true, 0);

    EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());
    EXPECT_EQ(3u, handler->ackCount);

    handler->ackCount = 0;
    tileSessions[0]->detachTile();
    // No ACK called on detach
    EXPECT_EQ(0u, handler->ackCount);
    EXPECT_EQ(10u, handler->debugEventAcked.seqno);
    EXPECT_EQ(uint32_t(PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND), handler->debugEventAcked.type);
}

TEST_F(TileAttachTest, GivenBlockingOnCpuAttachedTileAndZebinModulesWithEventsToAckWhenDetachingTileThenLastEventIsAcked) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_FALSE(rootSession->blockOnFenceMode);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->attachTile();

    addZebinVmBindEvent(rootSession, vm0, true, true, 0);
    addZebinVmBindEvent(rootSession, vm0, true, true, 1);
    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());

    addZebinVmBindEvent(rootSession, vm1, true, true, 0);
    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(2u, handler->ackCount);
    auto ackBeforeDetach = handler->ackCount;

    EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());
    tileSessions[0]->detachTile();
    EXPECT_EQ(ackBeforeDetach + 1u, handler->ackCount);
    EXPECT_EQ(10u, handler->debugEventAcked.seqno);
    EXPECT_EQ(uint32_t(PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND), handler->debugEventAcked.type);
}

TEST_F(TileAttachTest, GivenTileAttachedAndIsaWithOsEventToAckWhenDetachingTileThenAllEventsAreAcked) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->attachTile();

    addIsaVmBindEvent(rootSession, vm0, true, true);

    EXPECT_EQ(0u, handler->ackCount);
    rootSession->detachTileDebugSession(tileSessions[0]);

    EXPECT_EQ(1u, handler->ackCount);
    EXPECT_EQ(20u, handler->debugEventAcked.seqno);
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND), handler->debugEventAcked.type);

    auto isa = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->isaMap[0][isaGpuVa].get();
    EXPECT_EQ(0u, isa->ackEvents.size());
    EXPECT_TRUE(isa->moduleLoadEventAck);
}

TEST_F(TileAttachTest, GivenBlockingOnCpuAndZebinModuleEventWithoutAckWhenHandlingEventThenNoEventsToAckAdded) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_FALSE(rootSession->blockOnFenceMode);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->attachTile();

    addZebinVmBindEvent(rootSession, vm0, false, true, 0);
    addZebinVmBindEvent(rootSession, vm0, false, true, 1);

    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_FALSE(ZET_DEBUG_EVENT_FLAG_NEED_ACK & tileSessions[0]->apiEvents.front().flags);
    EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());

    EXPECT_EQ(0u, handler->ackCount);
}
using TileAttachBlockOnFenceTest = Test<TileAttachFixture<true>>;

TEST_F(TileAttachBlockOnFenceTest, GivenBlockingOnFenceDetachedTileAndZebinModulesWithEventsToAckWhenDetachingTileThenNoAckIoctlIsCalled) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_TRUE(rootSession->blockOnFenceMode);

    addZebinVmBindEvent(rootSession, vm0, true, true, 0);
    addZebinVmBindEvent(rootSession, vm0, true, true, 1);
    addZebinVmBindEvent(rootSession, vm1, true, true, 0);

    EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());
    EXPECT_EQ(3u, handler->ackCount);

    handler->ackCount = 0;
    tileSessions[0]->detachTile();
    // No ACK called on detach
    EXPECT_EQ(0u, handler->ackCount);
    EXPECT_EQ(uint32_t(PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND), handler->debugEventAcked.type);
}

TEST_F(TileAttachBlockOnFenceTest, GivenBlockingOnFenceAttachedTileAndZebinModulesWithEventsToAckWhenDetachingTileThenAllEventsAreAcked) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_TRUE(rootSession->blockOnFenceMode);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->attachTile();

    addZebinVmBindEvent(rootSession, vm0, true, true, 0);
    addZebinVmBindEvent(rootSession, vm0, true, true, 1);
    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(0u, handler->ackCount);

    addZebinVmBindEvent(rootSession, vm1, true, true, 0);
    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(ZET_DEBUG_EVENT_FLAG_NEED_ACK, tileSessions[0]->apiEvents.front().flags);
    EXPECT_EQ(1u, handler->ackCount);
    auto ackBeforeDetach = handler->ackCount;

    EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());
    EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[1].size());
    tileSessions[0]->detachTile();

    EXPECT_EQ(ackBeforeDetach + 2u, handler->ackCount);
    EXPECT_EQ(uint32_t(PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND), handler->debugEventAcked.type);
}

TEST_F(TileAttachBlockOnFenceTest, GivenBlockingOnFenceAttachedTileAndZebinModulesWithEventsToAckWhenModuleLoadEventIsAckedThenAllNewEventsAreAutoAcked) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_TRUE(rootSession->blockOnFenceMode);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->attachTile();

    addZebinVmBindEvent(rootSession, vm0, true, true, 0);
    addZebinVmBindEvent(rootSession, vm0, true, true, 1);
    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(0u, handler->ackCount);

    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());
    EXPECT_FALSE(rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].moduleLoadEventAcked[0]);
    EXPECT_EQ(2, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentVmBindCounter[0]);

    zet_debug_event_t event = {};
    auto result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_DEBUG_EVENT_FLAG_NEED_ACK, event.flags);

    result = zetDebugAcknowledgeEvent(tileSessions[0]->toHandle(), &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(2u, handler->ackCount);
    EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());

    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vm0 + 20] = 0;

    addZebinVmBindEvent(rootSession, vm0 + 20, true, true, 0);
    EXPECT_EQ(3u, handler->ackCount);
    EXPECT_EQ(3, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentVmBindCounter[0]);
    EXPECT_TRUE(rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].moduleLoadEventAcked[0]);

    EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].ackEvents[0].size());
}

TEST_F(TileAttachBlockOnFenceTest, GivenMultipleVmBindEventsForFirstZebinSegmentWhenHandlingEventThenLoadEventIsNotTriggered) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    EXPECT_TRUE(rootSession->blockOnFenceMode);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->attachTile();

    rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->vmToTile[vm0 + 20] = 0;

    addZebinVmBindEvent(rootSession, vm0, true, true, 0);
    addZebinVmBindEvent(rootSession, vm0 + 20, true, true, 0);
    EXPECT_EQ(0u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(2, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentVmBindCounter[0]);
    EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());

    addZebinVmBindEvent(rootSession, vm0 + 20, true, true, 1);
    EXPECT_EQ(1u, tileSessions[0]->apiEvents.size());
    EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[0].size());
}

using TileAttachAsyncThreadTest = Test<TileAttachFixture<>>;

TEST_F(TileAttachAsyncThreadTest, GivenInterruptedThreadsWhenNoAttentionEventIsReadThenThreadUnavailableEventIsGenerated) {
    rootSession->tileSessions[0].second = true;
    tileSessions[0]->returnTimeDiff = rootSession->interruptTimeout * 10;

    ze_device_thread_t thread = {0, 0, 0, 0};
    auto result = tileSessions[0]->interrupt(thread);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    rootSession->synchronousInternalEventRead = true;
    rootSession->startAsyncThread();

    while (rootSession->getInternalEventCounter < 2)
        ;

    rootSession->closeAsyncThread();

    zet_debug_event_t event = {};
    result = zetDebugReadEvent(tileSessions[0]->toHandle(), 0, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ZET_DEBUG_EVENT_TYPE_THREAD_UNAVAILABLE, event.type);
    EXPECT_EQ(0u, event.info.thread.thread.slice);
    EXPECT_EQ(0u, event.info.thread.thread.subslice);
    EXPECT_EQ(0u, event.info.thread.thread.eu);
    EXPECT_EQ(0u, event.info.thread.thread.thread);
}

TEST_F(TileAttachTest, GivenEventWithL0ZebinModuleWhenHandlingEventThenModuleLoadAndUnloadEventsAreReportedForLastKernel) {
    uint64_t isaGpuVa2 = 0x340000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

    rootSession->tileSessions[0].second = true;
    tileSessions[0]->isAttached = true;

    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    for (uint32_t tile = 0; tile < 2; tile++) {
        auto handler = new MockIoctlHandlerI915;
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
        vmBindIsa->client_handle = MockDebugSessionLinuxi915::mockClientHandle;
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
        EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule.size());
        EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[tile].size());

        // event not pushed to ack
        EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->isaMap[tile][isaGpuVa]->ackEvents.size());
        EXPECT_EQ(1, handler->ioctlCalled); // ACK
        EXPECT_EQ(vmBindIsa->base.seqno, handler->debugEventAcked.seqno);

        vmBindIsa->va_start = isaGpuVa2;
        vmBindIsa->base.seqno = 11;
        handler->ioctlCalled = 0;
        handler->debugEventAcked.seqno = 0;

        rootSession->handleEvent(&vmBindIsa->base);

        EXPECT_EQ(0u, rootSession->apiEvents.size());
        EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule.size());
        EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].loadAddresses[tile].size());
        EXPECT_EQ(2u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount);

        auto &isaMap = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->isaMap[tile];
        EXPECT_EQ(2u, isaMap.size());

        bool attachedAfterModuleLoaded = false;
        if (rootSession->tileSessions[tile].second == false) {
            zet_debug_session_handle_t session = nullptr;
            zet_debug_config_t config = {};
            config.pid = 0x1234;
            zetDebugAttach(neoDevice->getSubDevice(tile)->getSpecializedDevice<L0::Device>()->toHandle(), &config, &session);
            attachedAfterModuleLoaded = true;
        }

        if (attachedAfterModuleLoaded == false) {
            // event not pushed to ack
            EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->isaMap[tile][isaGpuVa2]->ackEvents.size());
            EXPECT_EQ(0, handler->ioctlCalled);
            EXPECT_EQ(0u, handler->debugEventAcked.seqno);

            EXPECT_FALSE(isaMap[isaGpuVa]->moduleLoadEventAck);
            EXPECT_FALSE(isaMap[isaGpuVa2]->moduleLoadEventAck);
        } else {

            // event not pushed to ack
            EXPECT_EQ(0u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->isaMap[tile][isaGpuVa2]->ackEvents.size());
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

        auto elfAddress = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].ptr;
        auto elfSize = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].dataSize;
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

        EXPECT_EQ(rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].ptr, event.info.module.moduleBegin);
        EXPECT_EQ(rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].ptr +
                      rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap[elfUUID].dataSize,
                  event.info.module.moduleEnd);
    }
}

TEST_F(TileAttachTest, GivenZebinModuleVmBindForModuleFromDifferentTileThenVmBindIsAutoacked) {
    auto handler = new MockIoctlHandlerI915;
    rootSession->ioctlHandler.reset(handler);

    auto &isaUuidData = rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidMap.find(isaUUID)->second;
    DeviceBitfield bitfield;
    bitfield.set(1);
    auto deviceBitfield = static_cast<uint32_t>(bitfield.to_ulong());
    memcpy(isaUuidData.data.get(), &deviceBitfield, sizeof(deviceBitfield));

    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);
    uint64_t vmHandle = vm0;

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 10;
    vmBindIsa->client_handle = MockDebugSessionLinuxi915::mockClientHandle;
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
    EXPECT_EQ(handler->ackCount, 0u);
    rootSession->handleEvent(&vmBindIsa->base);
    EXPECT_EQ(handler->ackCount, 1u);
}

TEST_F(TileAttachTest, GivenZebinModuleDestroyedBeforeAttachWhenAttachingThenModuleLoadEventIsNotReported) {
    uint64_t isaGpuVa2 = 0x340000;
    uint64_t vmBindIsaData[sizeof(prelim_drm_i915_debug_event_vm_bind) / sizeof(uint64_t) + 3 * sizeof(typeOfUUID)];
    prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);
    uint64_t vmHandle = vm1;

    vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE | PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
    vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
    vmBindIsa->base.seqno = 10;
    vmBindIsa->client_handle = MockDebugSessionLinuxi915::mockClientHandle;
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

    vmBindIsa->va_start = isaGpuVa2;
    vmBindIsa->base.seqno = 11;

    rootSession->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(1u, rootSession->clientHandleToConnection[MockDebugSessionLinuxi915::mockClientHandle]->uuidToModule.size());
    ASSERT_EQ(1u, tileSessions[1]->modules.size());
    EXPECT_EQ(isaGpuVa2, tileSessions[1]->modules.begin()->second.load);

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->va_start = isaGpuVa2;

    rootSession->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, rootSession->apiEvents.size());

    vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
    vmBindIsa->va_start = isaGpuVa;

    rootSession->handleEvent(&vmBindIsa->base);

    EXPECT_EQ(0u, tileSessions[1]->modules.size());
}

} // namespace ult
} // namespace L0
