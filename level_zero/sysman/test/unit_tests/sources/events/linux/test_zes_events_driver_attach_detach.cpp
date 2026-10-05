/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/events/linux/mock_events.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr int mockAttachDetachReadPipeFd = 8;
constexpr int mockAttachDetachWritePipeFd = 9;
constexpr zes_event_type_flags_t attachDetachEvents = ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH;
constexpr zes_event_type_flags_t driverEventsSentinel = 0xABCD;
const std::string enumeratedDevicePciPath("/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0");
const std::string unknownDevicePciPath("/devices/pci0000:00/0000:00:01.0/0000:01:00.0");
const std::string foreignDevicePciPath("/devices/pci0000:00/0000:00:03.0/0000:05:00.0");
const std::string enumeratedDevicePciAddress("0000:9a:00.0");
const std::string unknownDevicePciAddress("0000:01:00.0");

class MockLinuxSysmanDriverImpForAttachDetach : public PublicLinuxSysmanDriverImp {
  public:
    // The enumerated and the unknown devices are supported Intel GPUs, while the foreign device is not reported
    std::vector<zes_pci_address_t> pciAddresses = {{0, 0x9a, 0, 0}, {0, 0x01, 0, 0}};
    uint32_t getPciDeviceAddressesCallCount = 0u;

    std::vector<zes_pci_address_t> getPciDeviceAddresses() override {
        getPciDeviceAddressesCallCount++;
        return pciAddresses;
    }

    ze_result_t getPciDeviceProperties(uint32_t *pCount, zes_intel_driver_pci_device_properties_exp_t *pProperties) override {
        // Reading the PCI device properties wakes up the devices, so the events must not use it
        ADD_FAILURE() << "getPciDeviceProperties() must not be used for driver scoped attach/detach events";
        return ZE_RESULT_ERROR_UNKNOWN;
    }
};

class SysmanEventsDriverAttachDetachFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        // The syscalls are mocked only after the sysman device is created, so that its initialization uses the real ones
        pipeBackup = mockSysCallsPipe;
        writeBackup = mockSysCallsWrite;
        readBackup = mockSysCallsRead;

        writeCallCount = 0u;
        pollCallCount = 0u;

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        // Driver scoped attach/detach events are supported with xe KMD
        pSysmanKmdInterfaceOriginal = std::move(pLinuxSysmanImp->pSysmanKmdInterface);
        pLinuxSysmanImp->pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());

        pLinuxSysmanDriverImp = std::make_unique<MockLinuxSysmanDriverImpForAttachDetach>();
        pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
        driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pLinuxSysmanDriverImp.get());

        pUdevLib = std::make_unique<EventsUdevLibMock>();
        pUdevLib->allocateDeviceToReceiveDataResult = &udevDeviceDummy;
        pUdevLibOriginal = pLinuxSysmanDriverImp->pUdevLib;
        pLinuxSysmanDriverImp->pUdevLib = pUdevLib.get();

        pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pLinuxSysmanDriverImp.get());
        pEventsUtilOriginal = pLinuxSysmanDriverImp->pLinuxEventsUtil;
        pLinuxSysmanDriverImp->pLinuxEventsUtil = pEventsUtil.get();
        pEventsUtilForPoll = pEventsUtil.get();
        pUdevLibForPoll = pUdevLib.get();
        pDriverImpForPoll = pLinuxSysmanDriverImp.get();

        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        pEventsUtilForPoll = nullptr;
        pUdevLibForPoll = nullptr;
        pDriverImpForPoll = nullptr;
        pEventsUtil->pipeFd[0] = -1;
        pEventsUtil->pipeFd[1] = -1;
        pLinuxSysmanDriverImp->pLinuxEventsUtil = pEventsUtilOriginal;
        pEventsUtil.reset();

        pLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
        driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
        pLinuxSysmanDriverImp.reset();

        pLinuxSysmanImp->pSysmanKmdInterface = std::move(pSysmanKmdInterfaceOriginal);
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;

        SysmanDeviceFixture::TearDown();
    }

    void setUdevEvent(const char *eventType, const std::string &devPath) {
        pUdevLib->getEventTypeResult = eventType;
        pUdevLib->eventPropertyValueDevPathResult = devPath;
    }

    ze_result_t listenForDriverEvents(uint64_t timeout, uint32_t &numDeviceEvents, zes_event_type_flags_t &deviceEvents, zes_event_type_flags_t &driverEvents) {
        zes_device_handle_t phDevices[1] = {device->toHandle()};
        return zesDriverEventListenExt(driverHandle->toHandle(), timeout, 1u, phDevices, &numDeviceEvents, &deviceEvents, &driverEvents);
    }

    static int mockSysCallsPipe(int pipeFd[2]) {
        pipeFd[0] = mockAttachDetachReadPipeFd;
        pipeFd[1] = mockAttachDetachWritePipeFd;
        return 1;
    }

    static ssize_t mockSysCallsWrite(int fd, const void *buf, size_t count) {
        writeCallCount++;
        return 1;
    }

    static ssize_t mockSysCallsRead(int fd, void *buf, size_t count) {
        if (fd != mockAttachDetachReadPipeFd) {
            return 0;
        }
        memset(buf, 0, count);
        return static_cast<ssize_t>(count);
    }

    static int markFdReady(struct pollfd *pollFd, unsigned long int numberOfFds, int fd) {
        int readyCount = 0;
        for (unsigned long int i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == fd) {
                pollFd[i].revents = POLLIN;
                readyCount++;
            }
        }
        return readyCount;
    }

    static int mockPollUdevReady(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
        pollCallCount++;
        return markFdReady(pollFd, numberOfFds, mockUdevFd);
    }

    static inline uint32_t writeCallCount = 0u;
    static inline uint32_t pollCallCount = 0u;
    static inline PublicLinuxEventsUtil *pEventsUtilForPoll = nullptr;
    static inline EventsUdevLibMock *pUdevLibForPoll = nullptr;
    static inline MockLinuxSysmanDriverImpForAttachDetach *pDriverImpForPoll = nullptr;
    int udevDeviceDummy = 0;
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterfaceOriginal;
    std::unique_ptr<MockEventsSysfsAccess> pSysfsAccess;
    SysFsAccessInterface *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockLinuxSysmanDriverImpForAttachDetach> pLinuxSysmanDriverImp;
    std::unique_ptr<EventsUdevLibMock> pUdevLib;
    std::unique_ptr<PublicLinuxEventsUtil> pEventsUtil;
    L0::Sysman::OsSysmanDriver *pOsSysmanDriverOriginal = nullptr;
    L0::Sysman::UdevLib *pUdevLibOriginal = nullptr;
    L0::Sysman::LinuxEventsUtil *pEventsUtilOriginal = nullptr;
    VariableBackup<decltype(NEO::SysCalls::sysCallsPipe)> pipeBackup{&NEO::SysCalls::sysCallsPipe};
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> writeBackup{&NEO::SysCalls::sysCallsWrite};
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readBackup{&NEO::SysCalls::sysCallsRead};
};

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDevPathsWhenGettingPciAddressOfDrmCardNodeThenAddressIsReturnedOnlyForPrimaryDrmCardNodeOfPciDevice) {
    const std::vector<std::pair<std::string, std::string>> drmCardNodes = {{enumeratedDevicePciPath + "/drm/card0", enumeratedDevicePciAddress},
                                                                           {unknownDevicePciPath + "/drm/card12", unknownDevicePciAddress},
                                                                           {"/devices/pci0000:00/0000:00:02.0/drm/card1", "0000:00:02.0"},
                                                                           {"/devices/pci0001:ab/0001:ab:1f.7/drm/card3", "0001:ab:1f.7"}};
    for (const auto &[devPath, expectedPciAddress] : drmCardNodes) {
        std::string pciAddress;
        EXPECT_TRUE(PublicLinuxEventsUtil::getPciAddressOfDrmCardNode(devPath, pciAddress));
        EXPECT_EQ(expectedPciAddress, pciAddress);
    }

    const std::vector<std::string> notDrmCardNodes = {enumeratedDevicePciPath + "/drm/card",
                                                      enumeratedDevicePciPath + "/drm/card0/card0-DP-1",
                                                      enumeratedDevicePciPath + "/drm/renderD128",
                                                      enumeratedDevicePciPath + "/xe.nvm.768",
                                                      enumeratedDevicePciPath,
                                                      "/devices/pci0000:00/drm/card0",
                                                      "/devices/pci0000:00/0000:00:01.0/0000:03:00.0/simple-framebuffer.0/drm/card0",
                                                      "/devices/virtual/drm/card0",
                                                      ""};
    for (const auto &devPath : notDrmCardNodes) {
        std::string pciAddress;
        EXPECT_FALSE(PublicLinuxEventsUtil::getPciAddressOfDrmCardNode(devPath, pciAddress));
        EXPECT_TRUE(pciAddress.empty());
    }
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenPciDeviceAddressesWhenGettingSupportedPciDeviceAddressesThenAddressesAreFormattedAsInSysfs) {
    pLinuxSysmanDriverImp->pciAddresses = {{0, 0x9a, 0, 0}, {1, 0xab, 0x1f, 7}};
    std::set<std::string> pciAddresses = {"0000:05:00.0"};

    pEventsUtil->getSupportedPciDeviceAddresses(pciAddresses);
    const std::set<std::string> expectedPciAddresses = {enumeratedDevicePciAddress, "0001:ab:1f.7"};
    EXPECT_EQ(expectedPciAddresses, pciAddresses);
    EXPECT_EQ(1u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenNoSupportedPciDeviceWhenGettingSupportedPciDeviceAddressesThenNoAddressIsReturned) {
    pLinuxSysmanDriverImp->pciAddresses.clear();
    std::set<std::string> pciAddresses = {enumeratedDevicePciAddress};

    pEventsUtil->getSupportedPciDeviceAddresses(pciAddresses);
    EXPECT_TRUE(pciAddresses.empty());
    EXPECT_EQ(1u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAddressesAreOutdatedAndDetachIsRegisteredWhenUpdatingSupportedPciDeviceAddressesThenAddressesAreCollectedOnlyOnce) {
    pEventsUtil->supportedPciDeviceAddressesOutdated = true;

    pEventsUtil->updateSupportedPciDeviceAddresses(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);
    const std::set<std::string> expectedPciAddresses = {enumeratedDevicePciAddress, unknownDevicePciAddress};
    EXPECT_EQ(expectedPciAddresses, pEventsUtil->supportedPciDeviceAddresses);
    EXPECT_FALSE(pEventsUtil->supportedPciDeviceAddressesOutdated);
    EXPECT_EQ(1u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);

    pLinuxSysmanDriverImp->pciAddresses.clear();
    pEventsUtil->updateSupportedPciDeviceAddresses(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);
    EXPECT_EQ(expectedPciAddresses, pEventsUtil->supportedPciDeviceAddresses);
    EXPECT_EQ(1u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAddressesAreOutdatedAndDetachIsNotRegisteredWhenUpdatingSupportedPciDeviceAddressesThenPciDevicesAreNotQueried) {
    pEventsUtil->supportedPciDeviceAddressesOutdated = true;

    for (const auto &driverRegisteredEvents : {static_cast<zes_event_type_flags_t>(0), static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH), static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT)}) {
        pEventsUtil->updateSupportedPciDeviceAddresses(driverRegisteredEvents);
        EXPECT_TRUE(pEventsUtil->supportedPciDeviceAddresses.empty());
        EXPECT_TRUE(pEventsUtil->supportedPciDeviceAddressesOutdated);
    }
    EXPECT_EQ(0u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDetachRegistrationChangesWhenRegisteringDriverEventsThenAddressesAreMarkedOutdatedOnlyWhenDetachGetsRegistered) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    EXPECT_FALSE(pEventsUtil->supportedPciDeviceAddressesOutdated);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));
    EXPECT_TRUE(pEventsUtil->supportedPciDeviceAddressesOutdated);

    pEventsUtil->supportedPciDeviceAddressesOutdated = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_FALSE(pEventsUtil->supportedPciDeviceAddressesOutdated);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), 0));
    EXPECT_FALSE(pEventsUtil->supportedPciDeviceAddressesOutdated);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_TRUE(pEventsUtil->supportedPciDeviceAddressesOutdated);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsSupportedWhenRegisteringAttachAndDetachDriverEventsThenSuccessIsReturnedAndDeviceRegistrationsAreNotAffected) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), pEventsUtil->registeredDriverEvents);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH), pEventsUtil->registeredDriverEvents);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents | ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));
    EXPECT_EQ(attachDetachEvents | ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT, pEventsUtil->registeredDriverEvents);
    EXPECT_TRUE(pEventsUtil->deviceEventsMap.empty());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), 0));
    EXPECT_EQ(0u, pEventsUtil->registeredDriverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenI915KmdWhenRegisteringAttachOrDetachDriverEventsThenUnsupportedFeatureIsReturnedAndRegistrationIsNotUpdated) {
    pLinuxSysmanImp->pSysmanKmdInterface = std::make_unique<MockSysmanKmdInterfaceUpstream>(pLinuxSysmanImp->getSysmanProductHelper());
    pEventsUtil->pipeFd[1] = mockAttachDetachWritePipeFd;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH | ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));
    EXPECT_EQ(0u, pEventsUtil->registeredDriverEvents);
    EXPECT_EQ(0u, writeCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT), pEventsUtil->registeredDriverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsSupportedWhenRegisteringAttachDetachAlongWithDeviceScopedEventThenInvalidEnumerationIsReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents | ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    EXPECT_EQ(0u, pEventsUtil->registeredDriverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenListenIsInFlightWhenRegisteringAttachDetachDriverEventsThenPipeIsWritten) {
    pEventsUtil->pipeFd[1] = mockAttachDetachWritePipeFd;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));
    EXPECT_EQ(1u, writeCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));
    EXPECT_EQ(1u, writeCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDetachIsRegisteredWhenRemoveEventIsReceivedForDrmCardNodeOfUnknownDeviceThenDetachIsReportedInDriverEventsOnly) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("remove", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    uint32_t numDeviceEvents = 5u;
    zes_event_type_flags_t deviceEvents = driverEventsSentinel;
    zes_event_type_flags_t driverEvents = driverEventsSentinel;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, deviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), driverEvents);
    EXPECT_EQ(1u, pollCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachIsRegisteredWhenAddEventIsReceivedForDrmCardNodeOfUnknownDeviceThenAttachIsReportedInDriverEventsOnly) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("add", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, deviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH), driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenOnlyDetachIsRegisteredWhenAddEventIsReceivedForDrmCardNodeThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("add", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsRegisteredWhenChangeEventIsReceivedForDrmCardNodeThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("change", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsRegisteredWhenAddOrRemoveEventIsReceivedForNonDrmCardNodeThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    const std::vector<std::string> nonCardNodeDevPaths = {unknownDevicePciPath + "/drm/renderD128",
                                                          unknownDevicePciPath + "/drm/card1/card1-DP-1",
                                                          unknownDevicePciPath + "/xe.nvm.768",
                                                          "/devices/virtual/drm/card0"};
    for (const auto &eventType : {"add", "remove"}) {
        for (const auto &devPath : nonCardNodeDevPaths) {
            setUdevEvent(eventType, devPath);
            uint32_t numDeviceEvents = 0u;
            zes_event_type_flags_t deviceEvents = 0u;
            zes_event_type_flags_t driverEvents = 0u;
            EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
            EXPECT_EQ(0u, numDeviceEvents);
            EXPECT_EQ(0u, driverEvents);
        }
    }
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsRegisteredWhenUdevEventHasNoDevPathThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("remove", "");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsRegisteredWhenListeningWithZesDriverEventListenExThenDriverScopedEventIsNotListenedTo) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("remove", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    zes_event_type_flags_t deviceEvents = 0u;
    uint32_t numDeviceEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1000u, 1u, phDevices, &numDeviceEvents, &deviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, deviceEvents);
    // Nothing is registered for the device and the driver scoped events are ignored, so listen returns after the first poll
    EXPECT_EQ(1u, pollCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDetachIsRegisteredForDeviceAndDriverWhenRemoveEventIsReceivedForDrmCardNodeOfDeviceThenDetachIsReportedForBoth) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("remove", enumeratedDevicePciPath + "/drm/card0");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), deviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDetachIsRegisteredForDeviceOnlyWhenRemoveEventIsReceivedForDrmCardNodeOfDeviceThenDriverEventsIsNotReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("remove", enumeratedDevicePciPath + "/drm/card0");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = driverEventsSentinel;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), deviceEvents);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachIsRegisteredWhileListenIsInFlightWhenAddEventIsReceivedThenAttachIsReportedInDriverEvents) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        pollCallCount++;
        if (pollCallCount == 1u) {
            // Nothing is registered when listen starts, the registration done here wakes up the listen through the pipe
            EXPECT_EQ(ZE_RESULT_SUCCESS, pEventsUtilForPoll->driverEventRegister(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));
            return markFdReady(pollFd, numberOfFds, mockAttachDetachReadPipeFd);
        }
        pUdevLibForPoll->getEventTypeResult = "add";
        pUdevLibForPoll->eventPropertyValueDevPathResult = unknownDevicePciPath + "/drm/card1";
        return markFdReady(pollFd, numberOfFds, mockUdevFd);
    });

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(1u, writeCallCount);
    EXPECT_EQ(2u, pollCallCount);
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH), driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsClearedWhileListenIsInFlightThenListenReturnsWithoutDriverEvents) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        pollCallCount++;
        EXPECT_EQ(ZE_RESULT_SUCCESS, pEventsUtilForPoll->driverEventRegister(0));
        return markFdReady(pollFd, numberOfFds, mockAttachDetachReadPipeFd);
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(UINT64_MAX, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(1u, pollCallCount);
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenLibUdevIsNotAvailableAndAttachDetachIsRegisteredWhenListeningForDriverEventsThenNoEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    pLinuxSysmanDriverImp->pUdevLib = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = driverEventsSentinel;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
    EXPECT_EQ(0u, pollCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsRegisteredWhenAddOrRemoveEventIsReceivedForDrmCardNodeOfForeignDeviceThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    const std::vector<std::string> foreignDrmCardNodes = {foreignDevicePciPath + "/drm/card2",
                                                          "/devices/pci0000:00/0000:00:01.0/0000:03:00.0/simple-framebuffer.0/drm/card0"};
    for (const auto &eventType : {"add", "remove"}) {
        for (const auto &devPath : foreignDrmCardNodes) {
            setUdevEvent(eventType, devPath);
            uint32_t numDeviceEvents = 0u;
            zes_event_type_flags_t deviceEvents = 0u;
            zes_event_type_flags_t driverEvents = 0u;
            EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
            EXPECT_EQ(0u, numDeviceEvents);
            EXPECT_EQ(0u, driverEvents);
        }
    }
    const std::set<std::string> expectedPciAddresses = {enumeratedDevicePciAddress, unknownDevicePciAddress};
    EXPECT_EQ(expectedPciAddresses, pEventsUtil->supportedPciDeviceAddresses);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenAttachDetachIsRegisteredWhenRemoveEventIsReceivedForDeviceWhichWasNotPresentWhenListeningStartedThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    pLinuxSysmanDriverImp->pciAddresses = {{0, 0x9a, 0, 0}};
    setUdevEvent("remove", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenNoSupportedPciDeviceIsPresentWhenAddOrRemoveEventIsReceivedForDrmCardNodeThenNoDriverEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    pLinuxSysmanDriverImp->pciAddresses.clear();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), attachDetachEvents));

    for (const auto &eventType : {"add", "remove"}) {
        setUdevEvent(eventType, unknownDevicePciPath + "/drm/card1");
        uint32_t numDeviceEvents = 0u;
        zes_event_type_flags_t deviceEvents = 0u;
        zes_event_type_flags_t driverEvents = 0u;
        EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
        EXPECT_EQ(0u, driverEvents);
    }
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDeviceAppearsAtNewPciAddressWhenAddEventIsReceivedForItsDrmCardNodeThenAttachIsReportedAndAddressIsTracked) {
    const std::string newDevicePciPath("/devices/pci0000:00/0000:00:01.0/0000:07:00.0");
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        pollCallCount++;
        // The device shows up on the PCI bus only after the listening started
        pDriverImpForPoll->pciAddresses.push_back({0, 0x07, 0, 0});
        return markFdReady(pollFd, numberOfFds, mockUdevFd);
    });
    setUdevEvent("add", newDevicePciPath + "/drm/card3");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH), driverEvents);
    EXPECT_EQ(1u, pEventsUtil->supportedPciDeviceAddresses.count("0000:07:00.0"));
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenOnlyAttachIsRegisteredWhenRemoveEventIsReceivedForDrmCardNodeOfSupportedDeviceThenNoDriverEventIsReportedAndPciDevicesAreNotQueried) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    setUdevEvent("remove", unknownDevicePciPath + "/drm/card1");

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH));

    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, driverEvents);
    // The addresses are only needed to match the remove uevents, so they are not collected when only attach is registered
    EXPECT_TRUE(pEventsUtil->supportedPciDeviceAddresses.empty());
    EXPECT_EQ(0u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenTwoDevicesAreRemovedFromPciBusWhenSecondRemoveEventIsReceivedInNextListenThenDetachIsReportedForBoth) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    // The first listen returns on the remove uevent of the first device
    setUdevEvent("remove", enumeratedDevicePciPath + "/drm/card0");
    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), driverEvents);

    // Both devices are gone from the PCI bus by the time the remove uevent of the second device is received
    pLinuxSysmanDriverImp->pciAddresses.clear();
    setUdevEvent("remove", unknownDevicePciPath + "/drm/card1");
    driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), driverEvents);
    EXPECT_TRUE(pEventsUtil->supportedPciDeviceAddresses.empty());
    EXPECT_EQ(1u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

TEST_F(SysmanEventsDriverAttachDetachFixture, GivenDetachIsRegisteredAgainWhenRemoveEventIsReceivedThenItIsMatchedAgainstDevicesPresentWhenDetachWasRegisteredAgain) {
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, mockPollUdevReady);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    setUdevEvent("change", unknownDevicePciPath + "/drm/card1");
    uint32_t numDeviceEvents = 0u;
    zes_event_type_flags_t deviceEvents = 0u;
    zes_event_type_flags_t driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));

    // While detach is not registered, the device at 0000:01:00.0 is replaced by a device at 0000:07:00.0
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), 0));
    pLinuxSysmanDriverImp->pciAddresses = {{0, 0x07, 0, 0}};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventRegisterExt(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    setUdevEvent("remove", unknownDevicePciPath + "/drm/card1");
    driverEvents = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(0u, driverEvents);

    setUdevEvent("remove", "/devices/pci0000:00/0000:00:01.0/0000:07:00.0/drm/card3");
    EXPECT_EQ(ZE_RESULT_SUCCESS, listenForDriverEvents(1000u, numDeviceEvents, deviceEvents, driverEvents));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), driverEvents);
    EXPECT_EQ(2u, pLinuxSysmanDriverImp->getPciDeviceAddressesCallCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
