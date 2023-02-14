/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/events/linux/mock_events.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 2u;
constexpr int drmDeviceFd = 0;
class SysmanEventsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockEventsFsAccess> pFsAccess;
    std::unique_ptr<MockEventNeoDrm> pDrm;
    Drm *pOriginalDrm = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
    OsEvents *pOsEventsPrev = nullptr;
    L0::EventsImp *pEventsImp;
    std::unique_ptr<MockEventsSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockPmuInterfaceImpForEvents> pPmuInterface;
    PmuInterface *pOriginalPmuInterface = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    PublicLinuxEventsImp *pLinuxEventsImp = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEventsFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pDrm = std::make_unique<MockEventNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20>(*pDrm));
        pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_HBM2e);
        pLinuxSysmanImp->pDrm = pDrm.get();

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pEventsImp = static_cast<L0::EventsImp *>(pSysmanDeviceImp->pEvents);

        // Assign pUdevLib library handle to LinuxEventImp object, so that udev library handle methods could be mocked
        pLinuxSysmanImp->pUdevLib = new UdevLibMock();
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
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getRasHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        if (nullptr != pEventsImp->pOsEvents) {
            delete pEventsImp->pOsEvents;
        }
        pEventsImp->pOsEvents = pOsEventsPrev;
        pEventsImp = nullptr;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pDrm = pOriginalDrm;

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
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
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
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenPollSystemCallReturnsFailureWhenlisteningForResetRequiredEventThenDeviceListenReturnsNoEvents) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return -1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
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
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenValidSysmanHandleWhenDeviceEventListenIsInvokedThenVerifyEventListenReturnsFalse) {
    zes_event_type_flags_t events;
    EXPECT_FALSE(pSysmanDeviceImp->deviceEventListen(events, 1000u));
}

TEST_F(SysmanEventsFixture, GivenLibUdevNotFoundWhenListeningForEventsThenEventListenIsNotSuccess) {
    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    auto pLinuxEventsImp = new PublicLinuxEventsUtil();
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    VariableBackup<L0::LinuxEventsUtil *> eventsUtilBackup(&pPublicLinuxSysmanDriverImp->pLinuxEventsUtil);
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
    L0::osSysmanDriverDestructor();
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenNoEventsAreRegisteredWhenListeningForEventsThenVerifyListenSystemEventsReturnsFalse) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return -1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    auto pLinuxEventsImp = new PublicLinuxEventsUtil();
    auto pLinuxEventsUtilOld = pPublicLinuxSysmanDriverImp->pLinuxEventsUtil;
    VariableBackup<L0::LinuxEventsUtil *> eventsUtilBackup(&pPublicLinuxSysmanDriverImp->pLinuxEventsUtil);
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), 0));

    zes_event_type_flags_t pEvents;
    std::vector<zes_event_type_flags_t> registeredEvents;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    EXPECT_FALSE(pLinuxEventsImp->listenSystemEvents(&pEvents, 1u, registeredEvents, phDevices, 1000u));
    delete[] phDevices;
    pPublicLinuxSysmanDriverImp->pLinuxEventsUtil = pLinuxEventsUtilOld;
    L0::osSysmanDriverDestructor();
    delete pLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenOsSysmanDriverAsNullWhenListeningForEventsThenVerifyEventListenIsNotSuccess) {
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = nullptr;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenOsSysmanDriverAsNullWhenRegisteringForEventsThenVerifyEventListenIsNotSuccess) {
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = nullptr;
    zes_event_type_flags_t events = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEventRegister(device->toHandle(), events));
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenOsSysmanDriverAsNullWhenCallingDriverEventListenExThenVerifyEventListenIsNotSuccess) {
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = nullptr;
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
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
       GivenValidDeviceHandleAndListeningForEventsWhenEventGenerationSourceDeviceIsNotDrmAndPlatformThenEventListenReturnFalse) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenNoEventIsRegisteredThenVerifyNoEventsAreReturned) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
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
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleAndListeningEventsWhenNullEventTypeIsReceivedThenEventListenAPIReturnsNoEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";
    pUdevLibLocal->getEventTypeResult = nullptr;

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;
    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenUdevCallToAllocateDeviceFailsThenEventListenReturnFalse) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    pUdevLibLocal->allocateDeviceToReceiveDataResult = nullptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 10;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
    delete pMockFwInterface;
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenEventRegisterIsCalledThenSuccessIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfUeventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "0";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfDrmEventReceivedButChangeEventNotReceivedThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventTypeResult = "add"; // In order to receive RESET_REQUIRED event type must be "change"
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = nullptr;

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
    delete pMockFwInterface;
}

HWTEST2_F(SysmanEventsFixture,
          GivenValidDeviceHandleWhenListeningForResetRequiredEventsAfterInFieldRepairAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsResetEvent, IsPVC) {
    VariableBackup<FirmwareUtil *> backupFwUtil(&pLinuxSysmanImp->pFwUtilInterface);
    auto pMockFwInterface = new MockEventsFwInterface;
    pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface;

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    pMockFwInterface->mockIfrStatus = true;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    delete pMockFwInterface;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceDetachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";
    pUdevLibLocal->getEventTypeResult = "remove";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForDeviceDetachEventsAndDrmEventReceivedButRemoveEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";
    pUdevLibLocal->getEventTypeResult = "change"; // ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH will be received only if EventType is "remove"

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";
    pUdevLibLocal->getEventTypeResult = "add";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForDeviceAttachEventsAndDrmEventReceivedButAddEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";
    pUdevLibLocal->getEventTypeResult = "change"; // ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH will be received only if EventType is "add"

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenExAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";
    pUdevLibLocal->getEventTypeResult = "add";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForMemHealthEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_MEM_HEALTH, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndIfUeventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "0"; // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndDrmEventReceivedButChangeEventNotReceivedThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventTypeResult = "add";        // In order to receive MEM_HEALTH event type must be "change"
    pUdevLibLocal->getEventPropertyValueResult = "1"; // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return drmDeviceFd;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        buf->st_rdev = 0;
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = nullptr; // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    auto pPublicLinuxSysmanDriverImp = new PublicLinuxSysmanDriverImp();
    VariableBackup<L0::OsSysmanDriver *> driverBackup(&GlobalOsSysmanDriver);
    GlobalOsSysmanDriver = static_cast<L0::OsSysmanDriver *>(pPublicLinuxSysmanDriverImp);

    VariableBackup<L0::UdevLib *> udevBackup(&pPublicLinuxSysmanDriverImp->pUdevLib);
    pPublicLinuxSysmanDriverImp->pUdevLib = pUdevLibLocal;

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    L0::osSysmanDriverDestructor();
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventThenAfterReceivingEventRegisterEventAgainToReceiveEvent) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingCategoryThresholdEventAndCategoryErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingCategoryThresholdEventAndCategoryErrorsCrossesThresholdThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenRasGetStateReturnsFailureWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventThenEventListenAPIReturnsNoEvents) {
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
    pSysmanDeviceImp->pRasHandleContext->init(deviceHandles);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingTotalThresholdEventThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingTotalThresholdEventAndPmuReadSingleFailsThenEventListenAPIWaitForTimeoutIfEventNotReceived) {

    pPmuInterface->mockPmuReadFail = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(deviceHandles);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasUncorrectableErrorsCrossingTotalThresholdEventAndTotalErrorsDoNotThresholdThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForRasCorrectableErrorsCrossingCategoryThresholdEventAndCategoryErrorsDoNotCrossThresholdThenEventListenAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS));
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
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
} // namespace L0
