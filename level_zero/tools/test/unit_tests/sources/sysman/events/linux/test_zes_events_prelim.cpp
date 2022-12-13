/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/events/linux/mock_events_prelim.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

using ::testing::Matcher;

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 2u;
constexpr int fabricDeviceFd = 10;
constexpr int drmDeviceFd = 0;
class SysmanEventsFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockEventsFsAccess> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;
    OsEvents *pOsEventsPrev = nullptr;
    L0::EventsImp *pEventsImp;
    GlobalOperations *pGlobalOperationsOriginal = nullptr;
    std::unique_ptr<GlobalOperationsImp> pGlobalOperations;
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

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pEventsImp = static_cast<L0::EventsImp *>(pSysmanDeviceImp->pEvents);

        // Assign pUdevLib library handle to LinuxEventImp object, so that udev library handle methods could be mocked
        pLinuxSysmanImp->pUdevLib = new UdevLibMock();
        pLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
        pEventsImp->pOsEvents = pLinuxEventsImp;

        pGlobalOperations = std::make_unique<GlobalOperationsImp>(pLinuxSysmanImp);
        pGlobalOperationsOriginal = pSysmanDeviceImp->pGlobalOperations;
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperations.get();
        pSysmanDeviceImp->pGlobalOperations->init();

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
        pSysmanDeviceImp->pGlobalOperations = pGlobalOperationsOriginal;
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

TEST_F(SysmanEventsFixture, whenRegisteringEmptyEventsThenRegisteredEventsAreCleared) {
    zes_event_type_flags_t events = (ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxEventsImp->eventRegister(events));
    EXPECT_EQ(pLinuxEventsImp->registeredEvents, events);
    events = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxEventsImp->eventRegister(events));
    EXPECT_EQ(pLinuxEventsImp->registeredEvents, 0u);
}

TEST_F(SysmanEventsFixture, GivenLibUdevLibraryNotFoundWhenListeningForEventsThenEventListenIsNotSuccess) {
    auto pUdevLibBackup = pLinuxEventsImp->pUdevLib;
    pLinuxEventsImp->pUdevLib = nullptr;
    zes_event_type_flags_t pEvents;
    EXPECT_FALSE(pLinuxEventsImp->listenSystemEvents(pEvents, 1000u));
    pLinuxEventsImp->pUdevLib = pUdevLibBackup;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenEventGenerationSourceDeviceIsNotDrmAndPlatformThenEventListenReturnFalse) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 20;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenNoEventIsRegisteredThenEventListenReturnFalse) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 10;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

    // Step 4: Call APIs for validation
    pLinuxEventsImp->registeredEvents = 0;
    zes_event_type_flags_t pEvents;
    uint64_t timeout = 1u;
    EXPECT_TRUE(pLinuxEventsImp->listenSystemEvents(pEvents, timeout));

    // Step 5: Cleanup
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleAndListeningEventsWhenNullEventTypeIsReceivedThenEventListenAPIReturnsNoEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleAndListeningForEventsWhenUdevCallToAllocateDeviceFailsThenEventListenReturnFalse) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    pUdevLibLocal->allocateDeviceToReceiveDataResult = nullptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 10;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForResetRequiredEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenEventRegisterIsCalledThenSuccessIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfUeventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "0";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfDrmEventReceivedButChangeEventNotReceivedThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForResetRequiredEventsAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = nullptr;

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
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

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForFabricPortHealthEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });
    NEO::directoryFilesMap.insert({"/sys/devices/pci0000:00/0000:00:02.0", {"i915.iaf.31"}});

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = fabricDeviceFd;
    pUdevLibLocal->getEventPropertyValueResult = "PORT_CHANGE";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH, pDeviceEvents[0]);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForFabricEventsAndPlatformEventReceivedButPortChangeEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });
    NEO::directoryFilesMap.insert({"/sys/devices/pci0000:00/0000:00:02.0", {"msi_irqs", "iaf.31"}});

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "PORT_CHANGE_DUMMY"; // Fabric event will be considered only when Event property is PORT_CHANGE

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForFabricEventsAndPlatformEventReceivedButEventPropertyIsIncorrectThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });
    NEO::directoryFilesMap.insert({"/sys/devices/pci0000:00/0000:00:02.0", {"msi_irqs", "iaf.31"}});

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "PORT_CHANEE"; // Fabric event will be considered only when Event property is PORT_CHANGE

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForFabricEventsAndPlatformEventReceivedButEventPropertyIsNullThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });
    NEO::directoryFilesMap.insert({"/sys/devices/pci0000:00/0000:00:02.0", {"msi_irqs", "iaf.31"}});

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = nullptr; // Fabric event will be considered only when Event property is PORT_CHANGE

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForFabricEventsAndPlatformEventReceivedButEventTypeIsWrongThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });
    NEO::directoryFilesMap.insert({"/sys/devices/pci0000:00/0000:00:02.0", {"msi_irqs", "iaf.31"}});

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventTypeResult = "add";                  // FABRIC_PORT Event will be received only if EventType is "change"
    pUdevLibLocal->getEventPropertyValueResult = "PORT_CHANGE"; // Fabric event will be considered only when Event property is PORT_CHANGE

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

    // Step 4: Call APIs for validation
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH));
    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    // Step 5: Cleanup
    delete[] phDevices;
    delete[] pDeviceEvents;
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanEventsFixture, GivenFabricDeviceAttemptedToBeLookedIntoWrongDirectoryWhenReadingFabricDeviceStatsThenUnsupportedFeatureReturned) {
    std::string devicePciPath = "myDir";
    struct stat iafStat;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxEventsImp->readFabricDeviceStats(devicePciPath, iafStat));
}

TEST_F(SysmanEventsFixture, GivenFabricDeviceOpenFailedWhenReadingFabricDeviceStatsThenErrorReturned) {
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });
    NEO::directoryFilesMap.insert({"/sys/devices/pci0000:00/0000:00:02.0", {"msi_irqs", "iaf.31"}});
    std::string devicePciPath = "/sys/devices/pci0000:00/0000:00:02.0";
    struct stat iafStat;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, pLinuxEventsImp->readFabricDeviceStats(devicePciPath, iafStat));
    NEO::directoryFilesMap.clear();
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceDetachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForDeviceDetachEventsAndDrmEventReceivedButRemoveEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceWhenListeningForDeviceAttachEventsAndDrmEventReceivedButAddEventNotReceivedThenEventListenAPIWaitForTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenExAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForDeviceAttachEventsThenEventListenExAPIWaitForTimeoutIfEventNotReceived) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));

    pFsAccess->mockReadValZero = true;

    zes_device_handle_t *phDevices = new zes_device_handle_t[1];
    phDevices[0] = device->toHandle();
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pDeviceEvents = new zes_event_type_flags_t[1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    pFsAccess->mockReadValZero = false;
    pFsAccess->mockFileNotFoundError = true;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);

    delete[] phDevices;
    delete[] pDeviceEvents;
}

TEST_F(SysmanEventsFixture, GivenValidDeviceHandleWhenListeningForMemHealthEventsThenEventListenAPIReturnsAfterReceivingEventWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "1";

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndIfUeventReceivedWithWrongPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = "0"; // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndDrmEventReceivedButChangeEventNotReceivedThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
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

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
}

TEST_F(SysmanEventsFixture,
       GivenValidDeviceHandleWhenListeningForMemHealthEventsAndIfUeventReceivedWithNullPropertyValueThenEventListenAPIReturnsWithinTimeout) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        if (strcmp(pathname, "i915.iaf.31/iaf_fabric_id") == 0) {
            return fabricDeviceFd;
        } else {
            return drmDeviceFd;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsFstat)> mockFstat(&NEO::SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
        if (fd == fabricDeviceFd) {
            buf->st_rdev = 10;
        } else {
            buf->st_rdev = 0;
        }
        return 0;
    });

    // Step 1: Initialize a mocked udev lib object for this test case
    auto pUdevLibLocal = new UdevLibMock();
    int a = 0;
    void *ptr = &a; // Initialize a void pointer with dummy data
    pUdevLibLocal->allocateDeviceToReceiveDataResult = ptr;
    pUdevLibLocal->getEventGenerationSourceDeviceResult = 0;
    pUdevLibLocal->getEventPropertyValueResult = nullptr; // getEventPropertyValue must return 1 in order to assure that MEM_HEALTH event is there

    // Step 2: Create a new PublicLinuxEventsImp, where we will attach the above created Udev Lib object
    auto pPublicLinuxEventsImp = new PublicLinuxEventsImp(pOsSysman);
    pPublicLinuxEventsImp->pUdevLib = pUdevLibLocal;

    // Step 3: Backup original pOsEvent created during set up
    auto pOsEventOriginal = pEventsImp->pOsEvents;
    pEventsImp->pOsEvents = static_cast<OsEvents *>(pPublicLinuxEventsImp);

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
    pEventsImp->pOsEvents = pOsEventOriginal;
    delete pUdevLibLocal;
    delete pPublicLinuxEventsImp;
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
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
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
            setConfig.detailedThresholds.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS] = 11;
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
