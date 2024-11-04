/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/test/unit_tests/sources/events/linux/mock_events.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 2u;
constexpr int mockReadPipeFd = 8;
constexpr int mockWritePipeFd = 9;
constexpr uint32_t pmuDriverType = 16u;
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
    }

    void TearDown() override {
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

TEST_F(SysmanEventsFixture, whenRegisteringInvalidEventsThenErrorIsReturned) {
    zes_event_type_flags_t events = 0xFFFF;
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

TEST_F(SysmanEventsFixture, GivenNullDeviceHandleWhenListeningForEventsThenEventListenReturnsErrorUninitialized) {
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
    auto pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
    driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp.get());

    auto pUdevLibLocal = std::make_unique<EventsUdevLibMock>();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;

    auto pUdevLibOriginal = pPublicLinuxSysmanDriverImp->pUdevLib;
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal.get();

    std::unique_ptr<zes_device_handle_t[]> phDevices(new zes_device_handle_t[1]);
    phDevices[0] = nullptr;
    uint32_t numDeviceEvents = 0;
    std::unique_ptr<zes_event_type_flags_t[]> pDeviceEvents(new zes_event_type_flags_t[1]);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices.get(), &numDeviceEvents, pDeviceEvents.get()));

    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
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

    zes_event_type_flags_t pEvents;
    std::vector<zes_event_type_flags_t> registeredEvents;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    EXPECT_FALSE(pLinuxEventsImp->listenSystemEvents(&pEvents, 1u, registeredEvents, phDevices, 1000u));
    delete[] phDevices;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
    delete pPublicLinuxSysmanDriverImp;
    delete pLinuxEventsImp;
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
    EXPECT_FALSE(pLinuxEventsImp->listenSystemEvents(&pEvents, 1u, registeredEvents, phDevices, 1000u));
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
    pUdevLibLocal->getEventTypeResult = "change"; // ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH will be received only if EventType is "remove"

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

} // namespace ult
} // namespace Sysman
} // namespace L0
