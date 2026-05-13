/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_imp.h"
#include "level_zero/sysman/source/api/events/linux/sysman_os_events_netlink.h"
#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/events/linux/mock_events.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_nl_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/nl_api/mock_sysman_drm_nl_api.h"

#include <map>

namespace L0 {
namespace Sysman {
namespace ult {

static int netlinkSocketFdForPoll = 42;

static constexpr int netlinkTestReadPipeFd = 8;
static constexpr int netlinkTestWritePipeFd = 9;

void setupMockNlApiForSuccess(MockNlApi *mockNlApi, int socketFd = 42) {
    mockNlApi->mockLoadEntryPointsReturnValue.push_back(true);
    mockNlApi->mockNlSocketAllocReturnValue.push_back(reinterpret_cast<struct nl_sock *>(0x1234));
    mockNlApi->mockNlSocketAllocReturnValue.push_back(reinterpret_cast<struct nl_sock *>(0x5678)); // event socket
    mockNlApi->mockGenlConnectReturnValue.push_back(0);
    mockNlApi->mockGenCtrlResolveReturnValue.push_back(42);
    mockNlApi->mockGenlRegisterFamilyReturnValue.push_back(0);
    mockNlApi->mockGenlOpsResolveReturnValue.push_back(0);
    mockNlApi->mockGenlCtrlResolveGrpReturnValue.push_back(100);
    mockNlApi->mockNlSocketAddMembershipReturnValue.push_back(0);
    mockNlApi->mockNlSocketSetNonblockingReturnValue.push_back(0);
    mockNlApi->mockNlSocketGetFdReturnValue.push_back(socketFd);
}

class PublicLinuxSysmanDriverImpForNetlinkListen : public L0::Sysman::LinuxSysmanDriverImp {
  public:
    using LinuxSysmanDriverImp::pDrmNl;
    using LinuxSysmanDriverImp::pUdevLib;
};
class MockDrmNlApiForEvents : public MockDrmNlApi {
  public:
    MockDrmNlApiForEvents(std::string devId) : MockDrmNlApi(devId) {
        pNlApi = std::make_unique<MockNlApi>();
        mockNlApi = static_cast<MockNlApi *>(pNlApi.get());
        setupMockNlApiForSuccess(mockNlApi, mockSocketFd);
        isInitDone = true;
    }

    ~MockDrmNlApiForEvents() override = default;

    ze_result_t subscribeReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t pollEventReturnStatus = ZE_RESULT_SUCCESS;
    bool subscribedToEvents = false;
    int mockSocketFd = 42;
    DrmRasEvent mockEvent = {};
    MockNlApi *mockNlApi = nullptr;

    ze_result_t subscribeToEvents() override {
        if (subscribeReturnStatus == ZE_RESULT_SUCCESS) {
            subscribedToEvents = true;
        }
        return subscribeReturnStatus;
    }

    ze_result_t unsubscribeFromEvents() override {
        subscribedToEvents = false;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t pollEvent(DrmRasEvent &event) override {
        if (pollEventReturnStatus == ZE_RESULT_SUCCESS) {
            event = mockEvent;
        }
        return pollEventReturnStatus;
    }

    int getEventSocketFd() const override {
        return mockSocketFd;
    }

    bool isSubscribedToEvents() const override {
        return subscribedToEvents;
    }
};

class MockProductHelperForNetlink : public L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
  public:
    bool isNetlinkSupported = true;

    bool isNetlinkEventSupported() override {
        return isNetlinkSupported;
    }
};

class PublicLinuxSysmanDriverImpForNetlink : public PublicLinuxSysmanDriverImp {
  public:
    std::unique_ptr<MockDrmNlApiForEvents> pMockDrmNl = nullptr;

    DrmNlApi *getDrmNlApiHandle() {
        return pMockDrmNl.get();
    }
};

class MockNetlinkRasUtilForEvents : public NetlinkRasUtil {
  public:
    using NetlinkRasUtil::rasNodes;
    MockNetlinkRasUtilForEvents(zes_ras_error_type_t type, L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId)
        : NetlinkRasUtil(type, pLinuxSysmanImp, subdeviceId) {}
    ~MockNetlinkRasUtilForEvents() override = default;
};

class SysmanEventsNetlinkFixture : public SysmanDeviceFixture {
  protected:
    MockProductHelperForNetlink *pMockProductHelperImp = nullptr;
    std::unique_ptr<SysmanProductHelper> pOriginalProductHelper = nullptr;
    std::unique_ptr<PublicLinuxSysmanDriverImpForNetlink> pPublicLinuxSysmanDriverImp = nullptr;
    L0::Sysman::OsSysmanDriver *pOsSysmanDriverOriginal = nullptr;
    std::vector<DrmRasNode> originalRasNodes;

  public:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        originalRasNodes = MockNetlinkRasUtilForEvents::rasNodes;
        pOriginalProductHelper = std::move(pLinuxSysmanImp->pSysmanProductHelper);
        auto pMockProductHelper = std::make_unique<MockProductHelperForNetlink>();
        pMockProductHelperImp = pMockProductHelper.get();
        pLinuxSysmanImp->pSysmanProductHelper = std::move(pMockProductHelper);
        pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlink>();
        pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
        driverHandle->pOsSysmanDriver = pPublicLinuxSysmanDriverImp.get();
        pPublicLinuxSysmanDriverImp->pMockDrmNl = std::make_unique<MockDrmNlApiForEvents>("testCard");
    }

    void TearDown() override {
        MockNetlinkRasUtilForEvents::rasNodes = std::move(originalRasNodes);
        pLinuxSysmanImp->pSysmanProductHelper = std::move(pOriginalProductHelper);
        driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanEventsNetlinkFixture, GivenProductDoesNotSupportNetlinkWhenNetlinkInitializeIsCalledThenUnsupportedFeatureIsReturned) {
    DrmNlApi *pDrmNl = nullptr;
    pMockProductHelperImp->isNetlinkSupported = false;

    ze_result_t result = netlinkInitialize(pPublicLinuxSysmanDriverImp.get(), pDrmNl, pSysmanDeviceImp);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(pDrmNl, nullptr);
}

TEST_F(SysmanEventsNetlinkFixture, GivenPollEventFailsWhenNetlinkHandleEventsIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    pPublicLinuxSysmanDriverImp->pMockDrmNl->pollEventReturnStatus = ZE_RESULT_ERROR_UNKNOWN;

    bool result = netlinkHandleEvents(pPublicLinuxSysmanDriverImp->pMockDrmNl.get(),
                                      events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenCorrectableErrorEventWhenNetlinkHandleEventsIsCalledThenTrueIsReturnedAndEventIsSet) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    pPublicLinuxSysmanDriverImp->pMockDrmNl->mockEvent.nodeId = 0;
    pPublicLinuxSysmanDriverImp->pMockDrmNl->mockEvent.errorId = 1;

    bool result = netlinkHandleEvents(pPublicLinuxSysmanDriverImp->pMockDrmNl.get(),
                                      events, 1, devices, registeredEvents);

    EXPECT_TRUE(result);
    EXPECT_EQ(events[0], ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
}

TEST_F(SysmanEventsNetlinkFixture, GivenCorrectableErrorNodeWhenNetlinkProcessRasEventIsCalledThenCorrectEventTypeIsSet) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_TRUE(result);
    EXPECT_EQ(events[0], ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
}

TEST_F(SysmanEventsNetlinkFixture, GivenUncorrectableErrorNodeWhenNetlinkProcessRasEventIsCalledThenCorrectEventTypeIsSet) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {1, pLinuxSysmanImp->getDevicePciBdf(), "uncorrectable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 1;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_TRUE(result);
    EXPECT_EQ(events[0], ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS);
}

TEST_F(SysmanEventsNetlinkFixture, GivenUnknownNodeIdWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 999;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenDeviceNotRegisteredForEventTypeWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS}; // Registered for uncorrectable

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenMismatchedDeviceBdfWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, "0000:99:99.9", "correctable-errors", 1}}; // Different BDF
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);
    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNodeIdMismatchInRasNodeWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, "0000:99:99.9", "correctable-errors", 1},
        {1, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
    EXPECT_EQ(events[0], 0u);
}

TEST_F(SysmanEventsNetlinkFixture, GivenCorrectableErrorEventWhenProcessNetlinkRasEventIsCalledViaLinuxEventsUtilThenTrueIsReturnedAndEventIsSet) {
    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());

    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = pLinuxEventsUtil->processNetlinkRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_TRUE(result);
    EXPECT_EQ(events[0], ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
}

TEST_F(SysmanEventsNetlinkFixture, GivenValidDrmNlApiWhenNetlinkGetSocketFdIsCalledThenValidFdIsReturned) {
    int fd = netlinkGetSocketFd(pPublicLinuxSysmanDriverImp->pMockDrmNl.get());
    EXPECT_EQ(fd, 42);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNullDrmNlApiWhenNetlinkGetSocketFdIsCalledThenMinusOneIsReturned) {
    int fd = netlinkGetSocketFd(nullptr);
    EXPECT_EQ(fd, -1);
}

TEST_F(SysmanEventsNetlinkFixture, GivenSubscribedSocketWhenIsNetlinkSubscribedIsCalledThenTrueIsReturned) {
    pPublicLinuxSysmanDriverImp->pMockDrmNl->subscribedToEvents = true;
    bool result = isNetlinkSubscribed(pPublicLinuxSysmanDriverImp->pMockDrmNl.get());
    EXPECT_TRUE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenUnsubscribedSocketWhenIsNetlinkSubscribedIsCalledThenFalseIsReturned) {
    pPublicLinuxSysmanDriverImp->pMockDrmNl->subscribedToEvents = false;
    bool result = isNetlinkSubscribed(pPublicLinuxSysmanDriverImp->pMockDrmNl.get());
    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNullDrmNlApiWhenIsNetlinkSubscribedIsCalledThenFalseIsReturned) {
    bool result = isNetlinkSubscribed(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenMultipleDevicesWhenNetlinkProcessRasEventIsCalledThenOnlyMatchingDeviceGetsEvent) {
    zes_event_type_flags_t events[2] = {0, 0};
    zes_device_handle_t devices[2] = {pSysmanDeviceImp->toHandle(), pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {
        ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS,
        ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 2, devices, registeredEvents);

    EXPECT_TRUE(result);
    EXPECT_EQ(events[0], ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNullDeviceHandleWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {nullptr};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenInitializeRasNodesFailsWhenNetlinkInitializeIsCalledThenErrorIsReturned) {
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    MockNetlinkRasUtilForEvents::rasNodes.clear();
    static_cast<MockDrmNlApi *>(pMockDrmNl)->listNodesReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
    pDriverImp->pDrmNl = pMockDrmNl;

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pDriverImp.get(), pDrmNl, pSysmanDeviceImp);

    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenSubscribeToEventsFailsWhenNetlinkInitializeIsCalledThenErrorIsReturnedAndDrmNlIsNull) {
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_ERROR_UNKNOWN;

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    pDriverImp->pDrmNl = pMockDrmNl;

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pDriverImp.get(), pDrmNl, pSysmanDeviceImp);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(pDrmNl, nullptr);

    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenNullSysmanDeviceWhenNetlinkInitializeIsCalledThenProductHelperCheckIsSkippedAndInitSucceeds) {
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    pDriverImp->pDrmNl = pMockDrmNl;

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pDriverImp.get(), pDrmNl, nullptr);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pDrmNl, nullptr);

    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenUnsupportedNodeNameWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "unknown-error-type", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
}

TEST_F(SysmanEventsNetlinkFixture, GivenValidNetlinkEventWhenHandleNetlinkEventsIsCalledThenTrueIsReturnedAndEventIsSet) {
    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());
    pLinuxEventsUtil->pDrmNl = pPublicLinuxSysmanDriverImp->pMockDrmNl.get();

    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    pPublicLinuxSysmanDriverImp->pMockDrmNl->pollEventReturnStatus = ZE_RESULT_SUCCESS;
    pPublicLinuxSysmanDriverImp->pMockDrmNl->mockEvent.nodeId = 0;
    pPublicLinuxSysmanDriverImp->pMockDrmNl->mockEvent.errorId = 1;
    pPublicLinuxSysmanDriverImp->pMockDrmNl->subscribedToEvents = true;

    bool result = pLinuxEventsUtil->handleNetlinkEvents(events, 1, devices, registeredEvents);

    EXPECT_TRUE(result);
    EXPECT_EQ(events[0], ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNoNetlinkEventWhenHandleNetlinkEventsIsCalledThenFalseIsReturned) {
    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());
    pLinuxEventsUtil->pDrmNl = pPublicLinuxSysmanDriverImp->pMockDrmNl.get();

    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    pPublicLinuxSysmanDriverImp->pMockDrmNl->pollEventReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    pPublicLinuxSysmanDriverImp->pMockDrmNl->subscribedToEvents = true;

    bool result = pLinuxEventsUtil->handleNetlinkEvents(events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
    EXPECT_EQ(events[0], 0u);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNetlinkUnsupportedWhenNetlinkInitializeIsCalledThenUnsupportedFeatureIsReturned) {
    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());
    pMockProductHelperImp->isNetlinkSupported = false;

    DrmNlApi *pDrmNl = nullptr;
    ze_result_t result = netlinkInitialize(pPublicLinuxSysmanDriverImp.get(), pDrmNl, pSysmanDeviceImp);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    EXPECT_EQ(pDrmNl, nullptr);
}

TEST_F(SysmanEventsNetlinkFixture, GivenNetlinkInitializeSucceedsWhenInitRunsViaListenSystemEventsThenPDrmNlIsNotNull) {
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  return 0;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_NE(pLinuxEventsUtil->pDrmNl, nullptr);
    EXPECT_FALSE(result);

    pDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenNetlinkInitializeFailsListNodesWhenInitRunsViaListenSystemEventsThenPDrmNlIsNull) {
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    auto *pMockDrmNlBase = static_cast<MockDrmNlApi *>(pMockDrmNl);
    pMockDrmNlBase->listNodesReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    MockNetlinkRasUtilForEvents::rasNodes.clear();

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  return 0;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_EQ(pLinuxEventsUtil->pDrmNl, nullptr);
    EXPECT_FALSE(result);

    pDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenNullPhDevicesWhenListenSystemEventsCalledThenInitRunsWithNullDeviceAndReturnsFalse) {
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  return 0;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    std::vector<zes_event_type_flags_t> registeredEvents = {};

    // phDevices == nullptr and count == 0: pDevice will be nullptr inside init()
    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 0, registeredEvents, nullptr, 0);

    // pUdevLib is initialized (init ran), pDrmNl depends on netlinkInitialize with null device
    EXPECT_NE(pLinuxEventsUtil->pUdevLib, nullptr);
    EXPECT_FALSE(result);

    pDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenIsNetlinkSubscribedWhenListenSystemEventsCalledAndNetlinkHasNoEventAndUdevNotReadyThenReturnFalse) {
    constexpr int netlinkSocketFd = 42;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->mockSocketFd = netlinkSocketFd;
    pMockDrmNl->pollEventReturnStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  for (uint64_t i = 0; i < numberOfFds; i++) {
                                                                      if (pollFd[i].fd == 42) {
                                                                          pollFd[i].revents = POLLIN;
                                                                      } else {
                                                                          pollFd[i].revents = 0;
                                                                      }
                                                                  }
                                                                  return 1;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_FALSE(result);
    EXPECT_EQ(pEvents[0], 0u);

    pDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenNetlinkEventReadyWhenListenSystemEventsCalledThenReturnsTrueAndEventIsSet) {
    constexpr int netlinkSocketFd = 42;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->mockSocketFd = netlinkSocketFd;
    pMockDrmNl->pollEventReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->mockEvent.nodeId = 0;
    pMockDrmNl->mockEvent.errorId = 1;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pMockSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
    SysFsAccessInterface *pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });
    netlinkSocketFdForPoll = netlinkSocketFd;
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  for (unsigned long int i = 0; i < numberOfFds; i++) {
                                                                      if (pollFd[i].fd == netlinkSocketFdForPoll) {
                                                                          pollFd[i].revents = POLLIN;
                                                                      } else {
                                                                          pollFd[i].revents = 0;
                                                                      }
                                                                  }
                                                                  return 1;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(pEvents[0], ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);

    pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
    pDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenNetlinkEventReadyWhenHandleNetlinkEventsReturnsFalseThenListenSystemEventsReturnsFalse) {
    constexpr int netlinkSocketFd = 42;
    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->mockSocketFd = netlinkSocketFd;
    pMockDrmNl->pollEventReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pMockSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
    SysFsAccessInterface *pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });
    netlinkSocketFdForPoll = netlinkSocketFd;
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  for (unsigned long int i = 0; i < numberOfFds; i++) {
                                                                      if (pollFd[i].fd == netlinkSocketFdForPoll) {
                                                                          pollFd[i].revents = POLLIN;
                                                                      } else {
                                                                          pollFd[i].revents = 0;
                                                                      }
                                                                  }
                                                                  return 1;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_FALSE(result);
    EXPECT_EQ(pEvents[0], 0u);

    pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
    pDriverImp->pUdevLib = nullptr;
}

struct MockSysmanDeviceWithNullOsInterface : public L0::Sysman::SysmanDeviceImp {
    OsSysman *deviceGetOsInterface() override { return nullptr; }
};

TEST_F(SysmanEventsNetlinkFixture, GivenNullOsInterfaceWhenNetlinkProcessRasEventIsCalledThenFalseIsReturned) {
    MockSysmanDeviceWithNullOsInterface mockDevice;
    L0::Sysman::globalSysmanDriver->sysmanDevices.push_back(&mockDevice);

    zes_event_type_flags_t events[1] = {0};
    zes_device_handle_t devices[1] = {mockDevice.toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    std::vector<DrmRasNode> rasNodes = {
        {0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    DrmRasEvent event;
    event.nodeId = 0;
    event.errorId = 1;

    bool result = netlinkProcessRasEvent(event, events, 1, devices, registeredEvents);

    EXPECT_FALSE(result);
    EXPECT_EQ(events[0], 0u);

    auto &devs = L0::Sysman::globalSysmanDriver->sysmanDevices;
    devs.erase(std::remove(devs.begin(), devs.end(), &mockDevice), devs.end());
}

TEST_F(SysmanEventsNetlinkFixture, GivenUdevAndNetlinkBothReadySimultaneouslyWhenListenSystemEventsCalledThenBothEventsAreReturnedInSinglePass) {
    constexpr int netlinkSocketFd = 42;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();

    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->mockSocketFd = netlinkSocketFd;
    pMockDrmNl->pollEventReturnStatus = ZE_RESULT_SUCCESS;
    pMockDrmNl->mockEvent.nodeId = 0;
    pMockDrmNl->mockEvent.errorId = 1;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pMockSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
    SysFsAccessInterface *pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });

    netlinkSocketFdForPoll = netlinkSocketFd;
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
                                                                  int readyCount = 0;
                                                                  for (unsigned long int i = 0; i < numberOfFds; i++) {
                                                                      if (pollFd[i].fd == mockUdevFd || pollFd[i].fd == netlinkSocketFdForPoll) {
                                                                          pollFd[i].revents = POLLIN;
                                                                          readyCount++;
                                                                      } else {
                                                                          pollFd[i].revents = 0;
                                                                      }
                                                                  }
                                                                  return readyCount;
                                                              });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int fd) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_MEM_HEALTH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    bool result = pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(pEvents[0], static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_MEM_HEALTH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));

    pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
    pDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, WhenInitIsCalledThenUdevIsSetAndNetlinkIsNotInitialized) {
    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());
    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pPublicLinuxSysmanDriverImp->pUdevLib = pMockUdevLib.get();

    pLinuxEventsUtil->init();

    EXPECT_NE(pLinuxEventsUtil->pUdevLib, nullptr);
    EXPECT_EQ(pLinuxEventsUtil->pDrmNl, nullptr);

    pPublicLinuxSysmanDriverImp->pUdevLib = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenInitNetlinkSucceededWhenInitNetlinkCalledAgainThenDrmNlIsUnchanged) {
    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;
    pMockProductHelperImp->isNetlinkSupported = true;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());
    pLinuxEventsUtil->initNetlink();
    DrmNlApi *pFirst = pLinuxEventsUtil->pDrmNl;
    EXPECT_NE(pFirst, nullptr);

    pLinuxEventsUtil->initNetlink();
    EXPECT_NE(pLinuxEventsUtil->pDrmNl, nullptr);

    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenDeviceSupportsNetlinkWhenInitNetlinkIsCalledThenDrmNlIsSet) {
    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;
    pMockProductHelperImp->isNetlinkSupported = true;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());
    pLinuxEventsUtil->initNetlink();

    EXPECT_NE(pLinuxEventsUtil->pDrmNl, nullptr);

    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenNoDeviceSupportsNetlinkWhenInitNetlinkIsCalledThenDrmNlRemainsNull) {
    pMockProductHelperImp->isNetlinkSupported = false;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());
    pLinuxEventsUtil->initNetlink();

    EXPECT_EQ(pLinuxEventsUtil->pDrmNl, nullptr);
}

TEST_F(SysmanEventsNetlinkFixture, GivenEmptySysmanDevicesWhenInitNetlinkIsCalledThenDrmNlRemainsNull) {
    auto savedDevices = globalSysmanDriver->sysmanDevices;
    globalSysmanDriver->sysmanDevices.clear();

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pPublicLinuxSysmanDriverImp.get());
    pLinuxEventsUtil->initNetlink();

    EXPECT_EQ(pLinuxEventsUtil->pDrmNl, nullptr);

    globalSysmanDriver->sysmanDevices = std::move(savedDevices);
}

TEST_F(SysmanEventsNetlinkFixture, GivenFirstDeviceUnsupportedAndSecondDeviceSupportedWhenInitNetlinkIsCalledThenDrmNlIsSet) {
    class MockProductHelperWithCallCount : public L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
      public:
        mutable int callCount = 0;
        bool isNetlinkEventSupported() override {
            return (callCount++ > 0);
        }
    };

    auto pProductHelper = std::make_unique<MockProductHelperWithCallCount>();
    auto *pCountingHelperPtr = pProductHelper.get();
    auto pOriginalHelper = std::move(pLinuxSysmanImp->pSysmanProductHelper);
    pLinuxSysmanImp->pSysmanProductHelper = std::move(pProductHelper);

    globalSysmanDriver->sysmanDevices.push_back(pSysmanDeviceImp);

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());
    pLinuxEventsUtil->initNetlink();

    EXPECT_NE(pLinuxEventsUtil->pDrmNl, nullptr);
    EXPECT_GE(pCountingHelperPtr->callCount, 2);

    // Restore
    globalSysmanDriver->sysmanDevices.pop_back();
    pLinuxSysmanImp->pSysmanProductHelper = std::move(pOriginalHelper);

    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

TEST_F(SysmanEventsNetlinkFixture, GivenInitNetlinkFailedOnFirstListenWhenSecondListenCalledThenDrmNlRemainsNull) {
    // initNetlink() is called exactly once via call_once (initNetlinkOnce).
    // If it fails on the first listen (device unsupported), subsequent listens do NOT retry.
    pMockProductHelperImp->isNetlinkSupported = false;

    auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImpForNetlinkListen>();
    auto pMockDrmNl = new MockDrmNlApiForEvents("testCard");
    pMockDrmNl->subscribeReturnStatus = ZE_RESULT_SUCCESS;
    pDriverImp->pDrmNl = pMockDrmNl;

    auto pMockUdevLib = std::make_unique<EventsUdevLibMock>();
    pDriverImp->pUdevLib = pMockUdevLib.get();

    std::vector<DrmRasNode> rasNodes = {{0, pLinuxSysmanImp->getDevicePciBdf(), "correctable-errors", 1}};
    MockNetlinkRasUtilForEvents::rasNodes = rasNodes;

    auto pLinuxEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pDriverImp.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = netlinkTestReadPipeFd;
        pipeFd[1] = netlinkTestWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll,
                                                              [](struct pollfd *, unsigned long int, int) -> int { return 0; });
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose(&SysCalls::sysCallsClose, [](int) -> int { return 0; });

    zes_event_type_flags_t pEvents[1] = {0};
    zes_device_handle_t phDevices[1] = {pSysmanDeviceImp->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents = {ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS};

    pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);
    EXPECT_EQ(pLinuxEventsUtil->pDrmNl, nullptr);

    pMockProductHelperImp->isNetlinkSupported = true;
    pLinuxEventsUtil->listenSystemEvents(pEvents, 1, registeredEvents, phDevices, 0);

    EXPECT_EQ(pLinuxEventsUtil->pDrmNl, nullptr);

    pDriverImp->pUdevLib = nullptr;
    delete pDriverImp->pDrmNl;
    pDriverImp->pDrmNl = nullptr;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
