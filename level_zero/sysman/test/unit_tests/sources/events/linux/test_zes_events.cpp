/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/events/linux/mock_events.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 2u;
constexpr int mockReadPipeFd = 8;
constexpr int mockWritePipeFd = 9;
constexpr uint32_t pmuDriverType = 16u;

struct EventRegistrationDuringListen {
    static inline PublicLinuxEventsUtil *pEventsUtil = nullptr;
    static inline L0::Sysman::SysmanDeviceImp *pDevice = nullptr;
    static inline zes_event_type_flags_t deviceEvents = 0;
    static inline zes_event_type_flags_t driverEvents = 0;
    static inline uint32_t pollCount = 0;

    static int pipe(int pipeFd[2]) {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 0;
    }

    static int poll(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
        if (pollCount++ > 0) {
            return 0;
        }
        if (deviceEvents != 0) {
            pEventsUtil->eventRegister(deviceEvents, pDevice);
        }
        if (driverEvents != 0) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, pEventsUtil->driverEventRegister(driverEvents));
        }
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockReadPipeFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    }

    static void reset(PublicLinuxEventsUtil *pUtil, L0::Sysman::SysmanDeviceImp *pSysmanDevice, zes_event_type_flags_t deviceEventsToRegister, zes_event_type_flags_t driverEventsToRegister) {
        pEventsUtil = pUtil;
        pDevice = pSysmanDevice;
        deviceEvents = deviceEventsToRegister;
        driverEvents = driverEventsToRegister;
        pollCount = 0;
    }
};

class MockUdevLibRegisteringCperBeforeListenSetup : public EventsUdevLibMock {
  public:
    PublicLinuxEventsUtil *pEventsUtil = nullptr;

    int registerEventsFromSubsystemAndGetFd(std::vector<std::string> &subsystemList) override {
        EXPECT_EQ(ZE_RESULT_SUCCESS, pEventsUtil->driverEventRegister(ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));
        return mockUdevFd;
    }
};

class SysmanEventsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockEventsFsAccess> pFsAccess;
    MockEventNeoDrm *pDrm = nullptr;
    Drm *pOriginalDrm = nullptr;
    FsAccessInterface *pFsAccessOriginal = nullptr;
    OsEvents *pOsEventsPrev = nullptr;
    L0::Sysman::EventsImp *pEventsImp;
    std::unique_ptr<MockEventsSysfsAccess> pSysfsAccess;
    SysFsAccessInterface *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockPmuInterfaceImpForEvents> pPmuInterface;
    PmuInterface *pOriginalPmuInterface = nullptr;
    PublicLinuxEventsImp *pLinuxEventsImp = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<EventsUdevLibMock> pUdevLibMock;
    L0::Sysman::UdevLib *pUdevLibOriginal = nullptr;
    std::unique_ptr<SysmanProductHelper> pOriginalProductHelper;

    void setNetlinkEventSupport(bool supported) {
        if (this->pOriginalProductHelper == nullptr) {
            this->pOriginalProductHelper = std::move(this->pLinuxSysmanImp->pSysmanProductHelper);
        }
        auto pMockProductHelper = std::make_unique<MockProductHelperForNetlink>();
        pMockProductHelper->isNetlinkSupported = supported;
        this->pLinuxSysmanImp->pSysmanProductHelper = std::move(pMockProductHelper);
    }

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEventsFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pDrm = new MockEventNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockEventNeoDrm>(pDrm));

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pEventsImp = static_cast<L0::Sysman::EventsImp *>(pSysmanDeviceImp->pEvents);

        pLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
        pEventsImp->pOsEvents = pLinuxEventsImp;
        pPmuInterface = std::make_unique<MockPmuInterfaceImpForEvents>(pLinuxSysmanImp);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pFsAccess->mockReadValSuccess = true;

        for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pRasHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
        getRasHandles(0);

        auto pPublicLinuxSysmanDriverImp = static_cast<PublicLinuxSysmanDriverImp *>(this->driverHandle->pOsSysmanDriver);
        this->pUdevLibMock = std::make_unique<EventsUdevLibMock>();
        this->pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
        pPublicLinuxSysmanDriverImp->pUdevLib = this->pUdevLibMock.get();
    }

    void TearDown() override {
        if (this->pOriginalProductHelper != nullptr) {
            this->pLinuxSysmanImp->pSysmanProductHelper = std::move(this->pOriginalProductHelper);
        }
        static_cast<PublicLinuxSysmanDriverImp *>(this->driverHandle->pOsSysmanDriver)->pUdevLib = this->pUdevLibOriginal;
        if (nullptr != pEventsImp->pOsEvents) {
            delete pEventsImp->pOsEvents;
        }
        pEventsImp->pOsEvents = pOsEventsPrev;
        pEventsImp = nullptr;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;

        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanEventsFixture, GivenInvalidEventTypeFlagsWhenRegisteringEventsThenInvalidEnumerationErrorIsReturned) {
    zes_event_type_flags_t events = 0x10000;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, pLinuxEventsImp->eventRegister(events));
}

TEST_F(SysmanEventsFixture, GivenValidSysmanHandleWhenEventsAreClearedThenDeviceListenReturnsNoEvents) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    zes_event_type_flags_t events = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), events));
    zes_device_handle_t *phDevices = new zes_device_handle_t[2];
    phDevices[0] = device->toHandle();
    phDevices[1] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[2];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 2u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenPollSystemCallReturnsFailureWhenlisteningForResetRequiredEventThenDeviceListenReturnsNoEvents) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return -1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[2];
    phDevices[0] = device->toHandle();
    phDevices[1] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[2];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 2u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenDeviceHandleWhichCannotBeResolvedToASysmanDeviceWhenListeningForEventsThenInvalidArgumentIsReturned) {
    PublicLinuxSysmanDriverImp driverImp = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);

    VariableBackup<decltype(globalSysmanDriver)> globalSysmanDriverBackup(&globalSysmanDriver, nullptr);

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, eventsUtil->eventsListen(0u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenLiveAndStaleTracePipeDescriptorsWhenUpdatingCperPollSourcesThenStaleSourcesAreDroppedAndOnlyTheMissingOnesAreAdded) {
    constexpr int mockLiveTracePipeFd = 21;
    constexpr int mockUnpolledTracePipeFd = 22;
    constexpr int mockStaleTracePipeFd = 23;

    PublicLinuxSysmanDriverImp driverImp = {};
    driverImp.registerCperTracePipeFd(mockLiveTracePipeFd);
    driverImp.registerCperTracePipeFd(mockUnpolledTracePipeFd);
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);

    // A source that is not a trace pipe at all, a trace pipe the driver still knows about, and a trace
    // pipe whose collection instance has been deleted since the previous listen call.
    std::vector<PollDescriptor> pollSources = {
        {{mockReadPipeFd, POLLIN, 0}, PollSourceType::pipe},
        {{mockLiveTracePipeFd, POLLIN, 0}, PollSourceType::tracefs},
        {{mockStaleTracePipeFd, POLLIN, 0}, PollSourceType::tracefs}};

    bool cperRegistered = false;
    eventsUtil->updateCperPollSource(ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT, pollSources, cperRegistered);

    EXPECT_TRUE(cperRegistered);
    ASSERT_EQ(3u, pollSources.size());
    // Sources that are not trace pipes are left alone.
    EXPECT_EQ(mockReadPipeFd, pollSources[0].pfd.fd);
    EXPECT_EQ(PollSourceType::pipe, pollSources[0].type);
    // The still registered trace pipe keeps its existing source rather than being dropped and added again.
    EXPECT_EQ(mockLiveTracePipeFd, pollSources[1].pfd.fd);
    EXPECT_EQ(PollSourceType::tracefs, pollSources[1].type);
    // Only the registered trace pipe that was not being polled yet is appended, and the stale one is gone.
    EXPECT_EQ(mockUnpolledTracePipeFd, pollSources[2].pfd.fd);
    EXPECT_EQ(PollSourceType::tracefs, pollSources[2].type);
}

TEST_F(SysmanEventsFixture, GivenEventsUtilFailsWhenListeningForEventsThroughOsSysmanDriverThenErrorIsPropagatedAndSurvivabilityHandlingIsSkipped) {
    PublicLinuxSysmanDriverImp driverImp = {};

    VariableBackup<decltype(globalSysmanDriver)> globalSysmanDriverBackup(&globalSysmanDriver, nullptr);

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, driverImp.eventsListen(0u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_FALSE(pSysmanDeviceImp->isDeviceInSurvivabilityMode);
}

TEST_F(SysmanEventsFixture, GivenPipeSystemCallReturnsFailureWhenlisteningForResetRequiredEventThenDeviceListenReturnsResetRequiredEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        return -1;
    });

    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenPollSystemCallReturnsOnAllFdsWhenlisteningForResetRequiredEventThenDeviceListenReturnsResetRequiredEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });

    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            pollFd[i].revents = POLLIN;
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenPollSystemCallReturnsOnPipeFdWhenlisteningForResetRequiredEventThenEventListenAPIReturnsOnTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });

    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockReadPipeFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenValidSysmanHandleWhenDeviceEventListenIsInvokedThenVerifyEventListenReturnsFalse) {
    zes_event_type_flags_t events;
    EXPECT_FALSE(pSysmanDeviceImp->deviceEventListen(events, 1000u));
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundWhenListeningForEventsThenEventListenIsNotSuccess) {
    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pLinuxEventsImp = new PublicLinuxEventsUtil(pPublicLinuxSysmanDriverImp);
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));

    pLinuxEventsImp->pUdevLib = nullptr;

    zes_event_type_flags_t pEvents = 0;
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED);
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    bool eventReceived = true;
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pLinuxEventsImp->listenSystemEvents(&pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
    delete[] phDevices;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndNoUdevEventIsRegisteredWhenListeningForSystemEventsThenSuccessIsReturnedOnTimeout) {
    setNetlinkEventSupport(false);
    PublicLinuxSysmanDriverImp driverImp{};
    driverImp.pUdevLib = nullptr;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    for (auto events : {static_cast<zes_event_type_flags_t>(0), static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS)}) {
        std::vector<zes_event_type_flags_t> registeredEvents(1, events);
        zes_event_type_flags_t pEvents[1] = {};
        bool eventReceived = true;
        auto pollCalledBefore = SysCalls::pollFuncCalled;
        EXPECT_EQ(ZE_RESULT_SUCCESS, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1u, eventReceived));
        EXPECT_FALSE(eventReceived);
        EXPECT_EQ(0u, pEvents[0]);
        EXPECT_EQ(pollCalledBefore + 1, SysCalls::pollFuncCalled);
    }
    EXPECT_EQ(nullptr, pEventsUtil->pUdevLib);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundWhenCallingDriverEventListenExThenDependencyUnavailableErrorIsReturned) {
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {ZES_EVENT_TYPE_FLAG_MEM_HEALTH};
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDriverEventListenEx(driverHandle->toHandle(), 1000u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundWhenCallingDriverEventListenThenDependencyUnavailableErrorIsReturned) {
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndNoEventsAreRegisteredWhenCallingDriverEventListenExThenSuccessIsReturnedOnTimeout) {
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(pollCalledBefore + 1, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndCperDataAvailableIsRegisteredWhenCallingDriverEventListenExtThenSuccessIsReturnedOnTimeout) {
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() { return true; });
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    zes_event_type_flags_t driverEvents = ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT;
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenExt(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
    EXPECT_EQ(pollCalledBefore + 1, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenNetlinkEventsAreSupportedButNotAvailableAndOnlyRasEventsAreRegisteredWhenCallingDriverEventListenExThenDependencyUnavailableErrorIsReturned) {
    setNetlinkEventSupport(true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndOnlyRasEventsAreRegisteredWhenCallingDriverEventListenExThenSuccessIsReturnedOnTimeout) {
    setNetlinkEventSupport(false);
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(pollCalledBefore + 1, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndCperDataAvailableAndUdevDeviceEventAreRegisteredWhenCallingDriverEventListenExtThenDependencyUnavailableErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() { return true; });
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    for (auto udevDeviceEvent : std::initializer_list<zes_event_type_flags_t>{ZES_EVENT_TYPE_FLAG_DEVICE_DETACH, ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED,
                                                                              ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, ZES_EVENT_TYPE_FLAG_MEM_HEALTH, ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH,
                                                                              ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING}) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), 0));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), udevDeviceEvent | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));

        uint32_t numDeviceEvents = 0;
        zes_event_type_flags_t pDeviceEvents[1] = {};
        zes_event_type_flags_t driverEvents = 0;
        auto pollCalledBefore = SysCalls::pollFuncCalled;
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDriverEventListenExt(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
        EXPECT_EQ(0u, numDeviceEvents);
        EXPECT_EQ(0u, pDeviceEvents[0]);
        EXPECT_EQ(0u, driverEvents);
        EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
    }
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndCperDataAvailableAndRasEventsAreRegisteredWhenCallingDriverEventListenExtThenSuccessIsReturnedOnTimeout) {
    setNetlinkEventSupport(false);
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() { return true; });
    static_cast<PublicLinuxSysmanDriverImp *>(driverHandle->pOsSysmanDriver)->pUdevLib = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    zes_event_type_flags_t driverEvents = 0;
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenExt(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
    EXPECT_EQ(pollCalledBefore + 1, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndCperDataAvailableAndDriverAttachDetachAreRegisteredWhenListeningForSystemEventsThenDependencyUnavailableErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() { return true; });
    PublicLinuxSysmanDriverImp driverImp{};
    driverImp.pUdevLib = nullptr;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, 0);
    for (auto attachDetachEvent : {ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, ZES_EVENT_TYPE_FLAG_DEVICE_DETACH}) {
        pEventsUtil->registeredDriverEvents = ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT | attachDetachEvent;

        zes_event_type_flags_t pEvents[1] = {};
        zes_event_type_flags_t driverEvents = 0;
        bool eventReceived = true;
        auto pollCalledBefore = SysCalls::pollFuncCalled;
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1u, eventReceived, &driverEvents));
        EXPECT_FALSE(eventReceived);
        EXPECT_EQ(0u, driverEvents);
        EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
    }
    EXPECT_EQ(nullptr, pEventsUtil->pUdevLib);
}

TEST_F(SysmanEventsFixture, GivenCperDataAvailableIsRegisteredAndCperEventsAreNotSupportedWhenCallingDriverEventListenExtThenDependencyUnavailableErrorIsReturnedAndSupportIsCheckedOnce) {
    static uint32_t cperSupportCheckCount = 0;
    cperSupportCheckCount = 0;
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() {
        cperSupportCheckCount++;
        return false;
    });
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    for (uint32_t listenCount = 0; listenCount < 2; listenCount++) {
        uint32_t numDeviceEvents = 0;
        zes_event_type_flags_t pDeviceEvents[1] = {};
        zes_event_type_flags_t driverEvents = 0;
        auto pollCalledBefore = SysCalls::pollFuncCalled;
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesDriverEventListenExt(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
        EXPECT_EQ(0u, numDeviceEvents);
        EXPECT_EQ(0u, driverEvents);
        EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
    }
    EXPECT_EQ(1u, cperSupportCheckCount);
}

TEST_F(SysmanEventsFixture, GivenCperDataAvailableIsNotRegisteredAndCperEventsAreNotSupportedWhenCallingDriverEventListenExtThenSuccessIsReturnedWithoutCheckingCperSupport) {
    static uint32_t cperSupportCheckCount = 0;
    cperSupportCheckCount = 0;
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() {
        cperSupportCheckCount++;
        return false;
    });
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    zes_event_type_flags_t driverEvents = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenExt(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, cperSupportCheckCount);
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundWhenUdevEventIsRegisteredDuringListenThenDependencyUnavailableErrorIsReturnedAndPipeIsClosed) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, EventRegistrationDuringListen::pipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, EventRegistrationDuringListen::poll);
    setNetlinkEventSupport(false);
    PublicLinuxSysmanDriverImp driverImp{};
    driverImp.pUdevLib = nullptr;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    pEventsUtil->eventRegister(ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS, pSysmanDeviceImp);
    EventRegistrationDuringListen::reset(pEventsUtil.get(), pSysmanDeviceImp, ZES_EVENT_TYPE_FLAG_MEM_HEALTH, 0);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
    zes_event_type_flags_t pEvents[1] = {};
    bool eventReceived = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(1u, EventRegistrationDuringListen::pollCount);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[0]);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);
}

TEST_F(SysmanEventsFixture, GivenNetlinkEventsAreSupportedButNotAvailableWhenRasEventIsRegisteredDuringListenThenDependencyUnavailableErrorIsReturnedAndPipeIsClosed) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, EventRegistrationDuringListen::pipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, EventRegistrationDuringListen::poll);
    setNetlinkEventSupport(true);
    PublicLinuxSysmanDriverImp driverImp{};
    EventsUdevLibMock udevLib{};
    driverImp.pUdevLib = &udevLib;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    EventRegistrationDuringListen::reset(pEventsUtil.get(), pSysmanDeviceImp, ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS, 0);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, 0);
    zes_event_type_flags_t pEvents[1] = {};
    bool eventReceived = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(1u, EventRegistrationDuringListen::pollCount);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[0]);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);
    driverImp.pUdevLib = nullptr;
}

TEST_F(SysmanEventsFixture, GivenCperEventsAreNotSupportedWhenCperDataAvailableIsRegisteredDuringListenThenDependencyUnavailableErrorIsReturnedAndPipeIsClosed) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, EventRegistrationDuringListen::pipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, EventRegistrationDuringListen::poll);
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() { return false; });
    PublicLinuxSysmanDriverImp driverImp{};
    EventsUdevLibMock udevLib{};
    driverImp.pUdevLib = &udevLib;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    EventRegistrationDuringListen::reset(pEventsUtil.get(), pSysmanDeviceImp, 0, ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, 0);
    zes_event_type_flags_t pEvents[1] = {};
    zes_event_type_flags_t driverEvents = 0;
    bool eventReceived = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived, &driverEvents));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(0u, driverEvents);
    EXPECT_EQ(1u, EventRegistrationDuringListen::pollCount);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[0]);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);
    driverImp.pUdevLib = nullptr;
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundAndUdevEventIsRegisteredAfterRegisteredEventsWereReadWhenListeningForSystemEventsThenDependencyUnavailableErrorIsReturnedWithoutPolling) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, EventRegistrationDuringListen::pipe);
    setNetlinkEventSupport(false);
    PublicLinuxSysmanDriverImp driverImp{};
    driverImp.pUdevLib = nullptr;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    pEventsUtil->eventRegister(ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_MEM_HEALTH, pSysmanDeviceImp);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
    zes_event_type_flags_t pEvents[1] = {};
    bool eventReceived = true;
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[0]);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);
}

TEST_F(SysmanEventsFixture, GivenCperEventsAreNotSupportedAndCperDataAvailableIsRegisteredAfterDriverEventsWereReadWhenListeningForSystemEventsThenDependencyUnavailableErrorIsReturnedWithoutPolling) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, EventRegistrationDuringListen::pipe);
    VariableBackup<decltype(LinuxInfoLogImp::isCperEventSupported)> cperSupportBackup(&LinuxInfoLogImp::isCperEventSupported, []() { return false; });
    PublicLinuxSysmanDriverImp driverImp{};
    MockUdevLibRegisteringCperBeforeListenSetup udevLib{};
    driverImp.pUdevLib = &udevLib;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    udevLib.pEventsUtil = pEventsUtil.get();

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, 0);
    zes_event_type_flags_t pEvents[1] = {};
    zes_event_type_flags_t driverEvents = 0;
    bool eventReceived = true;
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived, &driverEvents));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(0u, driverEvents);
    EXPECT_EQ(pollCalledBefore, SysCalls::pollFuncCalled);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[0]);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);
    driverImp.pUdevLib = nullptr;
}

TEST_F(SysmanEventsFixture, GivenLibUdevIsAvailableWhenUdevEventIsRegisteredDuringListenThenListenContinuesUntilTimeoutAndSuccessIsReturned) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, EventRegistrationDuringListen::pipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, EventRegistrationDuringListen::poll);
    PublicLinuxSysmanDriverImp driverImp{};
    EventsUdevLibMock udevLib{};
    driverImp.pUdevLib = &udevLib;
    auto pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    EventRegistrationDuringListen::reset(pEventsUtil.get(), pSysmanDeviceImp, ZES_EVENT_TYPE_FLAG_MEM_HEALTH, 0);

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, 0);
    zes_event_type_flags_t pEvents[1] = {};
    bool eventReceived = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pEventsUtil->listenSystemEvents(pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived));
    EXPECT_FALSE(eventReceived);
    EXPECT_EQ(2u, EventRegistrationDuringListen::pollCount);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[0]);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);
    driverImp.pUdevLib = nullptr;
}

TEST_F(SysmanEventsFixture, GivenLibUdevIsAvailableAndNoEventIsReceivedWhenCallingDriverEventListenExThenSuccessIsReturnedOnTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[1] = {};
    auto pollCalledBefore = SysCalls::pollFuncCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(pollCalledBefore + 1, SysCalls::pollFuncCalled);
}

TEST_F(SysmanEventsFixture, GivenNoEventsAreRegisteredWhenListeningForEventsThenVerifyListenSystemEventsReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return -1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    auto pLinuxEventsImp = new PublicLinuxEventsUtil(pPublicLinuxSysmanDriverImp);
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), 0));

    zes_event_type_flags_t pEvents;
    std::vector<zes_event_type_flags_t> registeredEvents(1);
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    bool eventReceived = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxEventsImp->listenSystemEvents(&pEvents, 1u, registeredEvents, phDevices, 1000u, eventReceived));
    EXPECT_FALSE(eventReceived);
    delete[] phDevices;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenOsSysmanDriverAsNullWhenListeningForEventsThenVerifyEventListenIsNotSuccess) {
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver);
    driverHandle->pOsSysmanDriver = nullptr;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenOsSysmanDriverAsNullWhenRegisteringForEventsThenVerifyEventListenIsNotSuccess) {
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver);
    driverHandle->pOsSysmanDriver = nullptr;
    zes_event_type_flags_t events = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEventRegister(device->toHandle(), events));
}

TEST_F(SysmanEventsFixture, GivenOsSysmanDriverAsNullWhenCallingDriverEventListenExThenVerifyEventListenIsNotSuccess) {
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver);
    driverHandle->pOsSysmanDriver = nullptr;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForCurrentlyUnsupportedEventsThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_TEMP_THRESHOLD2));

    pFsAccess->mockReadValOne = true;

    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenEventGeneratedDevicePathIsNullThenEventListenReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->eventPropertyValueDevPathResult.clear();

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenNoEventIsRegisteredThenVerifyNoEventsAreReturned) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    zes_device_handle_t *phDevices = new zes_device_handle_t[2];
    phDevices[0] = device->toHandle();
    phDevices[1] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[2];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 2u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleAndListeningEventsWhenNullEventTypeIsReceivedThenEventListenAPIReturnsNoEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = nullptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenUdevCallToAllocateDeviceFailsThenEventListenReturnFalse) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    pUdevLibLocal->allocateDeviceToReceiveDataResult = nullptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 10;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = std::numeric_limits<uint32_t>::max();
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenNeedsResetUeventPropertyIsSignalledWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult = "NEEDS_RESET";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenEventRegisterIsCalledThenSuccessIsReturned) {
    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pLinuxEventsImp = new PublicLinuxEventsUtil(pPublicLinuxSysmanDriverImp);
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    pLinuxEventsImp->pipeFd[0] = mockReadPipeFd;
    pLinuxEventsImp->pipeFd[1] = mockWritePipeFd;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(pSysmanDeviceImp->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));

    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenEventsAreRegisteredWhenEventRegisterWithNoEventsIsCalledAgainThenSuccessIsReturned) {
    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pLinuxEventsImp = new PublicLinuxEventsUtil(pPublicLinuxSysmanDriverImp);
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    pLinuxEventsImp->pipeFd[0] = mockReadPipeFd;
    pLinuxEventsImp->pipeFd[1] = mockWritePipeFd;
    std::map<SysmanDeviceImp *, zes_event_type_flags_t> deviceEventsMap = {{pLinuxSysmanImp->getSysmanDeviceImp(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_TEMP_THRESHOLD2}};

    pLinuxEventsImp->deviceEventsMap = deviceEventsMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), 0));
    EXPECT_EQ(0u, pLinuxEventsImp->deviceEventsMap[pLinuxSysmanImp->getSysmanDeviceImp()]);

    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenImproperDevPathForUeventWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->eventPropertyValueDevPathResult = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenInvalidEventTypeWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventTypeResult = "Invalid";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenRealPathSystemCallFailsWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    pSysfsAccess->getRealPathResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenRealPathSystemCallReturnsInvalidDeviceWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    pSysfsAccess->realPath = "Invalid";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenEventPropertyForTypeKeyIsNullPtrWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->eventPropertyValueTypeResult = "";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenEventPropertyForTypeKeyInvalidWhenListeningForFabricHealthEventsThenEventListenAPIReturnsAfterTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->eventPropertyValueTypeResult = "Invalid";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenEventsAreRegisteredWhenEventRegisterIsCalledAgainThenSuccessIsReturned) {
    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pLinuxEventsImp = new PublicLinuxEventsUtil(pPublicLinuxSysmanDriverImp);
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    pLinuxEventsImp->pipeFd[0] = mockReadPipeFd;
    pLinuxEventsImp->pipeFd[1] = mockWritePipeFd;
    std::map<SysmanDeviceImp *, zes_event_type_flags_t> deviceEventsMap = {{pLinuxSysmanImp->getSysmanDeviceImp(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_TEMP_THRESHOLD2}};

    pLinuxEventsImp->deviceEventsMap = deviceEventsMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));

    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenWriteSystemCallReturnsFailureWhenEventRegisterIsCalledThenSuccessIsReturned) {
    VariableBackup<decltype(SysCalls::sysCallsWrite)> mockWrite(&SysCalls::sysCallsWrite, [](int fd, const void *buf, size_t count) -> ssize_t {
        return -1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pLinuxEventsImp = new PublicLinuxEventsUtil(pPublicLinuxSysmanDriverImp);
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    pLinuxEventsImp->pipeFd[0] = mockReadPipeFd;
    pLinuxEventsImp->pipeFd[1] = mockWritePipeFd;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));

    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfUeventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult = "0";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfDrmEventReceivedButChangeEventNotReceivedThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "add"; // In order to receive RESET_REQUIRED event type must be "change"

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult.clear();

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

HWTEST2_F(SysmanEventsFixture,
          GivenValidDeviceHandleWhenListeningForResetRequiredEventsAfterInFieldRepairAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsResetEvent, IsPVC) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    pMockFwInterface->mockIfrStatus = true;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceDetachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "remove";
    pUdevLibLocal->eventPropertyValueDevPathResult = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0/drm/card0";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForDeviceDetachEventsAndDrmEventReceivedButRemoveEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "change"; // ZES_EVENT_TYPE_FLAG_DEVICE_DETACH will be received only if EventType is "remove"
    pUdevLibLocal->eventPropertyValueDevPathResult = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0/drm/card0";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "add";
    pUdevLibLocal->eventPropertyValueDevPathResult = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0/drm/card0";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForDeviceAttachEventsAndDrmEventReceivedButAddEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "change"; // ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH will be received only if EventType is "add"
    pUdevLibLocal->eventPropertyValueDevPathResult = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0/drm/card0";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenExAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "add";
    pUdevLibLocal->eventPropertyValueDevPathResult = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0/drm/card0";

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenDevPathsOfChildNodesWhenCheckingForDrmCardNodeThenTrueIsReturnedOnlyForPrimaryDrmCardNode) {
    const std::string pciDevPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";
    EXPECT_TRUE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath + "/drm/card0", pciDevPath));
    EXPECT_TRUE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath + "/drm/card12", pciDevPath));
    EXPECT_FALSE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath + "/drm/card", pciDevPath));
    EXPECT_FALSE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath + "/drm/card0/card0-DP-1", pciDevPath));
    EXPECT_FALSE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath + "/drm/renderD128", pciDevPath));
    EXPECT_FALSE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath + "/xe.nvm.768", pciDevPath));
    EXPECT_FALSE(PublicLinuxEventsUtil::isDrmCardNode(pciDevPath, pciDevPath));
    EXPECT_FALSE(PublicLinuxEventsUtil::isDrmCardNode("/devices/pci0000:00/0000:00:02.0/drm/card0", pciDevPath));
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachAndDetachEventsAndAddOrRemoveEventIsReceivedForNonDrmCardNodeThenNoEventIsReturned) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t phDevices[1] = {device->toHandle()};

    const std::string pciDevPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";
    const std::vector<std::string> childNodeDevPaths = {pciDevPath + "/drm/renderD128",
                                                        pciDevPath + "/drm/card0/card0-DP-1",
                                                        pciDevPath + "/xe.nvm.768"};
    for (const auto &eventType : {"add", "remove"}) {
        for (const auto &childNodeDevPath : childNodeDevPaths) {
            pUdevLibLocal->getEventTypeResult = eventType;
            pUdevLibLocal->eventPropertyValueDevPathResult = childNodeDevPath;
            uint32_t numDeviceEvents = 0;
            zes_event_type_flags_t pDeviceEvents[1] = {0};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
            EXPECT_EQ(0u, numDeviceEvents);
            EXPECT_EQ(0u, pDeviceEvents[0]);
        }
    }

    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForMemHealthEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_MEM_HEALTH, pDeviceEvents[0]);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndIfUeventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult = "0"; // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndDrmEventReceivedButChangeEventNotReceivedThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "add"; // In order to receive MEM_HEALTH event type must be "change"

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult.clear(); // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 14;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventThenAfterReceivingEventRegisterEventAgainToReceiveEvent, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 14;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS, pDeviceEvents[0]);
    numDeviceEvents = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 14;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingCategoryThresholdEventAndCategoryErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 0;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            setConfig.detailedThresholds.category[ZES_RAS_ERROR_CAT_RESET] = 12;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingCategoryThresholdEventAndCategoryErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 0;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            setConfig.detailedThresholds.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS] = 11;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS, pDeviceEvents[0]);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventThenEventListenAPIWaitForTimeoutIfEventNotReceived, IsPVC) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 0;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenRasGetStateReturnsFailureWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventThenEventListenAPIReturnsNoEvents, IsPVC) {
    pFsAccess->mockListDirectoryResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;
    pMockFwInterface->fwGetMemoryErrorCountResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(pOsSysman->getSubDeviceCount());
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 0;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
    delete pMockFwInterface;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingTotalThresholdEventThenEventListenAPIWaitForTimeoutIfEventNotReceived, IsPVC) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 0;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingTotalThresholdEventAndPmuReadSingleFailsThenEventListenAPIWaitForTimeoutIfEventNotReceived, IsPVC) {

    pPmuInterface->mockPmuReadFail = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(pOsSysman->getSubDeviceCount());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 10;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsDoNotThresholdThenEventListenAPIWaitForTimeoutIfEventNotReceived, IsPVC) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 30;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

HWTEST2_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingCategoryThresholdEventAndCategoryErrorsDoNotCrossThresholdThenEventListenAPIWaitForTimeoutIfEventNotReceived, IsPVC) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        if (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 0;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            setConfig.detailedThresholds.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS] = 35;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        }
    }
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForUnsupportedEventTypeThenEventListenAPIReturnsWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FREQ_THROTTLED));

    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenNullOsInterfaceWhenBuildingDevPathMapThenUnrecoverableIsCalled) {
    PublicLinuxSysmanDriverImp driverImp{};
    EventsUdevLibMock udevLibLocal{};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;

    // Register an event so the device is not skipped by the early !registeredEvents check
    eventsUtil->deviceEventsMap[pSysmanDeviceImp] = ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED;

    std::vector<zes_event_type_flags_t> registeredEvents(1, 0);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath;
    L0::Sysman::FsAccessInterface *pFsAccess = nullptr;

    zes_device_handle_t handle = device->toHandle();
    // Null the OS interface so the null check inside getDevIndexToDevPathMap is triggered
    VariableBackup<L0::Sysman::OsSysman *> osInterfaceBackup(&pSysmanDeviceImp->pOsSysman, nullptr);

    // In test builds abortExecution() throws std::exception rather than calling abort()
    EXPECT_THROW(eventsUtil->getDevIndexToDevPathMap(registeredEvents, 1u, &handle, mapOfDevIndexToDevPath, pFsAccess), std::exception);
}

TEST_F(SysmanEventsFixture, GivenNullFsAccessWhenWedgedEventPathReachesIsSurvivabilityModeAsExpectedThenNoEventIsReturned) {
    PublicLinuxSysmanDriverImp driverImp = {};
    EventsUdevLibMock udevLibLocal = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;
    eventsUtil->action = "change";
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};

    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {{0u, "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0"}};
    L0::Sysman::FsAccessInterface *pFsAccess = nullptr;
    zes_event_type_flags_t pEvents[1] = {0};

    int a = 0;
    void *dev = &a;
    bool retVal = eventsUtil->checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pFsAccess, pEvents, dev, phDevices.data());
    EXPECT_FALSE(retVal);
    EXPECT_EQ(0u, pEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenDeviceAlreadyInSurvivabilityModeWhenEventsListenIsCalledThenNoEventIsReturnedWithoutUeventNotification) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED));
    pSysmanDeviceImp->isDeviceInSurvivabilityMode = true;

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    // Use timeout=0 so listenSystemEvents returns immediately with no udev event
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 0u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);

    pSysmanDeviceImp->isDeviceInSurvivabilityMode = false;
}

TEST_F(SysmanEventsFixture, GivenActionIsNotChangeWhenCheckingWedgedEventThenFalseIsReturned) {
    PublicLinuxSysmanDriverImp driverImp = {};
    EventsUdevLibMock udevLibLocal = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;
    eventsUtil->action = "add";

    const std::string devPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {{0u, devPath}};
    zes_event_type_flags_t pEvents[1] = {0};

    int a = 0;
    void *dev = &a;
    bool retVal = eventsUtil->checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pFsAccess.get(), pEvents, dev, phDevices.data());
    EXPECT_FALSE(retVal);
    EXPECT_EQ(0u, pEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenWedgedPropertyIsNullWhenCheckingWedgedEventThenFalseIsReturnedWithoutCrash) {
    PublicLinuxSysmanDriverImp driverImp = {};
    EventsUdevLibMock udevLibLocal = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;
    eventsUtil->action = "change";
    // Empty string causes mock to return nullptr for WEDGED property
    udevLibLocal.getEventPropertyValueResult = "";

    const std::string devPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {{0u, devPath}};
    zes_event_type_flags_t pEvents[1] = {0};

    int a = 0;
    void *dev = &a;
    bool retVal = eventsUtil->checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pFsAccess.get(), pEvents, dev, phDevices.data());
    EXPECT_FALSE(retVal);
    EXPECT_EQ(0u, pEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenWedgedPropertyIsVendorSpecificAndSurvivabilityModeIsRuntimeWhenCheckingWedgedEventThenEventFlagIsSetAndTrueIsReturned) {
    PublicLinuxSysmanDriverImp driverImp = {};
    EventsUdevLibMock udevLibLocal = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;
    eventsUtil->action = "change";
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockSurvivabilityModeEnabled = true;
    const std::string devPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {{0u, devPath}};
    zes_event_type_flags_t pEvents[1] = {0};

    int a = 0;
    void *dev = &a;
    bool retVal = eventsUtil->checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pFsAccess.get(), pEvents, dev, phDevices.data());
    EXPECT_TRUE(retVal);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED, pEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenWedgedPropertyIsVendorSpecificAndSurvivabilityModeIsNotRuntimeWhenCheckingWedgedEventThenFalseIsReturnedAndNoFlagIsSet) {
    PublicLinuxSysmanDriverImp driverImp = {};
    EventsUdevLibMock udevLibLocal = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;
    eventsUtil->action = "change";
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockSurvivabilityModeEnabled = false;
    const std::string devPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {{0u, devPath}};
    zes_event_type_flags_t pEvents[1] = {0};

    int a = 0;
    void *dev = &a;
    bool retVal = eventsUtil->checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pFsAccess.get(), pEvents, dev, phDevices.data());
    EXPECT_FALSE(retVal);
    EXPECT_EQ(0u, pEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenWedgedPropertyIsVendorSpecificAndSurvivabilityModeSysfsReadFailsWhenCheckingWedgedEventThenFalseIsReturnedAndNoFlagIsSet) {
    PublicLinuxSysmanDriverImp driverImp = {};
    EventsUdevLibMock udevLibLocal = {};
    auto eventsUtil = std::make_unique<PublicLinuxEventsUtil>(&driverImp);
    eventsUtil->pUdevLib = &udevLibLocal;
    eventsUtil->action = "change";
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockSurvivabilityModeReadFail = true;
    const std::string devPath = "/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0";

    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    std::vector<zes_event_type_flags_t> registeredEvents(1, ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED);
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {{0u, devPath}};
    zes_event_type_flags_t pEvents[1] = {0};

    int a = 0;
    void *dev = &a;
    bool retVal = eventsUtil->checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pFsAccess.get(), pEvents, dev, phDevices.data());
    EXPECT_FALSE(retVal);
    EXPECT_EQ(0u, pEvents[0]);
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForSurvivabilityModeEventAndSysfsNodeReadsOneAndWedgedPropertyIsVendorSpecificThenEventListenAPIReturnsEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockSurvivabilityModeEnabled = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED, pDeviceEvents[0]);
    EXPECT_TRUE(pSysmanDeviceImp->isDeviceInSurvivabilityMode);
    pSysmanDeviceImp->isDeviceInSurvivabilityMode = false;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForSurvivabilityModeEventAndSysfsNodeReadsZeroThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    // survivability_mode sysfs node reads 0 - event should be suppressed
    pFsAccess->mockSurvivabilityModeEnabled = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForSurvivabilityModeEventAndSysfsNodeReadFailsThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    // survivability_mode sysfs read fails - event should be suppressed
    pFsAccess->mockSurvivabilityModeReadFail = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForSurvivabilityModeEventAndSysfsNodeReadsOneButWedgedPropertyIsNotVendorSpecificThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    // WEDGED property is not "vendor-specific" - event should not fire
    udevLibLocal.getEventPropertyValueResult = "not-vendor-specific";

    pFsAccess->mockSurvivabilityModeEnabled = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenInvalidDeviceRealPathWhenListeningForSurvivabilityModeEventThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockSurvivabilityModeEnabled = true;
    // Real path has no "/devices" substring - device not added to dev path map
    pSysfsAccess->realPath = "/completely/invalid/path/without-device-substring";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenPowerOffPendingEventFlagWhenRegisteringEventsThenSuccessIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxEventsImp->eventRegister(ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING));
}

// The alert reason node is exposed by the xe driver only.
class SysmanEventsXeFixture : public SysmanEventsFixture {
  protected:
    MockSysmanKmdInterfaceXe *pSysmanKmdInterfaceXe = nullptr;
    std::unique_ptr<L0::Sysman::SysmanKmdInterface> pSysmanKmdInterfaceOriginal;

    void SetUp() override {
        SysmanEventsFixture::SetUp();
        pSysmanKmdInterfaceOriginal = std::move(pLinuxSysmanImp->pSysmanKmdInterface);
        pSysmanKmdInterfaceXe = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterfaceXe);
    }

    void TearDown() override {
        pLinuxSysmanImp->pSysmanKmdInterface = std::move(pSysmanKmdInterfaceOriginal);
        SysmanEventsFixture::TearDown();
    }
};

TEST_F(SysmanEventsXeFixture, GivenAlertReasonIsReportedWhenListeningForPowerOffPendingEventThenEventListenAPIReturnsEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockAlertReason = "Thermal Trip";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING), pDeviceEvents[0]);
}

TEST_F(SysmanEventsXeFixture, GivenAlertReasonNodeReportsNoneWhenListeningForPowerOffPendingEventThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    // Without an alert reason the wedged uevent is a survivability mode entry only.
    pFsAccess->mockAlertReason = "none";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsXeFixture, GivenAlertReasonIsReportedButWedgedPropertyIsNotVendorSpecificWhenListeningForPowerOffPendingEventThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "not-vendor-specific";

    pFsAccess->mockAlertReason = "Thermal Trip";

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsXeFixture, GivenAlertReasonNodeNameIsUnavailableWhenListeningForPowerOffPendingEventThenEventListenAPIDoesNotReturnEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());
    EventsUdevLibMock udevLibLocal = {};
    VariableBackup<L0::Sysman::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib, &udevLibLocal);
    udevLibLocal.allocateDeviceToReceiveDataResult = reinterpret_cast<void *>(0x1);
    udevLibLocal.getEventPropertyValueResult = "vendor-specific";

    pFsAccess->mockAlertReason = "Thermal Trip";
    pSysmanKmdInterfaceXe->mockNodeFileNameUnavailable = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_INTEL_EVENT_TYPE_EXP_FLAG_DEVICE_POWER_OFF_PENDING));
    std::vector<zes_device_handle_t> phDevices = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture, GivenNullDeviceHandleWhenCallingDriverEventListenThenInvalidArgumentIsReturned) {
    auto pPublicLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
    VariableBackup<L0::Sysman::OsSysmanDriver *> driverBackup(&driverHandle->pOsSysmanDriver, pPublicLinuxSysmanDriverImp.get());

    std::vector<zes_device_handle_t> phDevices = {nullptr};
    uint32_t numDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> pDeviceEvents(1, 0);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.data(), &numDeviceEvents, pDeviceEvents.data()));
    EXPECT_EQ(0u, numDeviceEvents);
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndWedgedEventReceivedButActionIsNotChangeThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventTypeResult = "add";                 // action must be "change" for wedged event
    pUdevLibLocal->getEventPropertyValueResult = "cold-reset"; // WEDGED property value

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndWedgedEventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult.clear(); // WEDGED property returns null

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndWedgedEventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult = "warm-reset"; // Wrong value, not "cold-reset"

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndWedgedEventReceivedWithColdResetValueThenEventListenAPIReturnsWithDeviceResetRequiredEvent) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockReadPipeFd;
        pipeFd[1] = mockWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == mockUdevFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pUdevLibLocal = new EventsUdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventPropertyValueResult = "cold-reset"; // WEDGED property value is "cold-reset"

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    // Verify that the wedged state was cached
    EXPECT_TRUE(pLinuxSysmanImp->isDeviceInWedgedState);

    delete[] phDevices;
    delete[] pDeviceEvents;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pUdevLibLocal;
    delete pMockFwInterface;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
