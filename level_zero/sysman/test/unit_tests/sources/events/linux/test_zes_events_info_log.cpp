/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/events/linux/mock_events.h"
#include "level_zero/sysman/test/unit_tests/sources/info_log/linux/mock_sysman_info_log.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t infoLogHandleCount = 1u;
constexpr int mockInfoLogReadPipeFd = 8;
constexpr int mockInfoLogWritePipeFd = 9;
constexpr int mockReopenedTracePipeFd = 210;
constexpr int mockTracefsFd = 211;
constexpr zes_event_type_flags_t driverSlotSentinel = 0xABCD;

class MockLinuxEventsUtilWithUnrequestedTracefsSource : public PublicLinuxEventsUtil {
  public:
    using PublicLinuxEventsUtil::PublicLinuxEventsUtil;

    void updateCperPollSource(zes_event_type_flags_t driverRegisteredEvents, std::vector<L0::Sysman::PollDescriptor> &pollSources, bool &cperRegistered) override {
        PublicLinuxEventsUtil::updateCperPollSource(driverRegisteredEvents, pollSources, cperRegistered);
        pollSources.push_back({{mockTracefsFd, POLLIN, 0}, L0::Sysman::PollSourceType::tracefs});
        cperRegisteredAfterUpdate = cperRegistered;
        updateCperPollSourceCallCount++;
    }

    uint32_t updateCperPollSourceCallCount = 0u;
    bool cperRegisteredAfterUpdate = false;
};

class SysmanEventsInfoLogFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        allowFakeDevicePathBackup = true;

        loadFuncBackup = mockLoadFunc;
        createTraceFsApiBackup = mockCreateTraceFsApi;

        openBackup = MockTraceFsApiWithData::mockSysCallsOpen;
        readBackup = mockSysCallsRead;
        closeBackup = MockTraceFsApiWithData::mockSysCallsClose;

        writeCallCount = 0u;
        writeReturnValue = 1;
        writeBackup = mockSysCallsWrite;

        pollCallCount = 0u;
        pipeReadCallCount = 0u;

        pLinuxSysmanDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
        pOsSysmanDriverOriginal = driverHandle->pOsSysmanDriver;
        driverHandle->pOsSysmanDriver = static_cast<L0::Sysman::OsSysmanDriver *>(pLinuxSysmanDriverImp.get());

        pUdevLib = std::make_unique<EventsUdevLibMock>();
        pUdevLibOriginal = pLinuxSysmanDriverImp->pUdevLib;
        pLinuxSysmanDriverImp->pUdevLib = pUdevLib.get();

        pEventsUtil = std::make_unique<PublicLinuxEventsUtil>(pLinuxSysmanDriverImp.get());
        pEventsUtilOriginal = pLinuxSysmanDriverImp->pLinuxEventsUtil;
        pLinuxSysmanDriverImp->pLinuxEventsUtil = pEventsUtil.get();

        pEventsUtilForPoll = pEventsUtil.get();
        pDriverImpForPoll = pLinuxSysmanDriverImp.get();

        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        pEventsUtilForPoll = nullptr;
        pDriverImpForPoll = nullptr;
        pEventsUtil->pipeFd[0] = -1;
        pEventsUtil->pipeFd[1] = -1;
        pLinuxSysmanDriverImp->pLinuxEventsUtil = pEventsUtilOriginal;
        pEventsUtil.reset();

        pLinuxSysmanDriverImp->pUdevLib = pUdevLibOriginal;
        pLinuxSysmanDriverImp->setCperTracePipeFd(-1);
        driverHandle->pOsSysmanDriver = pOsSysmanDriverOriginal;
        pLinuxSysmanDriverImp.reset();

        SysmanDeviceFixture::TearDown();
    }

    zes_intel_info_log_handle_t getInfoLogHandle() {
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
        EXPECT_EQ(infoLogHandleCount, count);

        zes_intel_info_log_handle_t hInfoLog = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, &hInfoLog));
        return hInfoLog;
    }

    int getCperTracePipeFd() {
        return pLinuxSysmanDriverImp->getCperTracePipeFd();
    }

    void seedDriverScopedRegistration(zes_event_type_flags_t events) {
        pEventsUtil->driverEventRegister(events);
    }

    zes_event_type_flags_t getDriverRegisteredEvents() {
        return pEventsUtil->registeredDriverEvents;
    }

    static void dropDriverScopedRegistrationFromPoll() {
        pEventsUtilForPoll->registeredDriverEvents = 0;
    }

    void startListenOnPipe() {
        pEventsUtil->pipeFd[0] = mockInfoLogReadPipeFd;
        pEventsUtil->pipeFd[1] = mockInfoLogWritePipeFd;
    }

    static NEO::OsLibrary *mockLoadFunc(const NEO::OsLibraryCreateProperties &) {
        return new MockTraceFsOsLibrary();
    }

    static std::unique_ptr<TraceFsApi> mockCreateTraceFsApi() {
        return std::make_unique<MockTraceFsApiWithData>();
    }

    static ssize_t mockSysCallsWrite(int fd, const void *buf, size_t count) {
        writeCallCount++;
        return writeReturnValue;
    }

    static ssize_t mockSysCallsRead(int fd, void *buf, size_t count) {
        if (fd == mockInfoLogReadPipeFd) {
            pipeReadCallCount++;
            memset(buf, 0, count);
            return static_cast<ssize_t>(count);
        }
        return MockTraceFsApiWithData::mockSysCallsRead(fd, buf, count);
    }

    static int mockSysCallsPipe(int pipeFd[2]) {
        pipeFd[0] = mockInfoLogReadPipeFd;
        pipeFd[1] = mockInfoLogWritePipeFd;
        return 1;
    }

    static void recordPollCall(struct pollfd *pollFd, unsigned long int numberOfFds) {
        if (pollCallCount < maxPollCalls) {
            size_t fdCount = (numberOfFds < maxPollFds) ? static_cast<size_t>(numberOfFds) : maxPollFds;
            polledFdCount[pollCallCount] = fdCount;
            for (size_t i = 0; i < fdCount; i++) {
                polledFds[pollCallCount][i] = pollFd[i].fd;
            }
        }
        pollCallCount++;
    }

    static bool wasFdPolled(size_t callIndex, int fd) {
        if (callIndex >= pollCallCount || callIndex >= maxPollCalls) {
            return false;
        }
        for (size_t i = 0; i < polledFdCount[callIndex]; i++) {
            if (polledFds[callIndex][i] == fd) {
                return true;
            }
        }
        return false;
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

    static constexpr size_t maxPollCalls = 4u;
    static constexpr size_t maxPollFds = 8u;
    static inline uint32_t writeCallCount = 0u;
    static inline ssize_t writeReturnValue = 1;
    static inline size_t pollCallCount = 0u;
    static inline uint32_t pipeReadCallCount = 0u;
    static inline size_t polledFdCount[maxPollCalls] = {};
    static inline int polledFds[maxPollCalls][maxPollFds] = {};
    static inline PublicLinuxEventsUtil *pEventsUtilForPoll = nullptr;
    static inline PublicLinuxSysmanDriverImp *pDriverImpForPoll = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<PublicLinuxSysmanDriverImp> pLinuxSysmanDriverImp;
    std::unique_ptr<EventsUdevLibMock> pUdevLib;
    std::unique_ptr<PublicLinuxEventsUtil> pEventsUtil;
    L0::Sysman::OsSysmanDriver *pOsSysmanDriverOriginal = nullptr;
    L0::Sysman::UdevLib *pUdevLibOriginal = nullptr;
    L0::Sysman::LinuxEventsUtil *pEventsUtilOriginal = nullptr;
    VariableBackup<bool> allowFakeDevicePathBackup{&NEO::SysCalls::allowFakeDevicePath};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup{&NEO::OsLibrary::loadFunc};
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup{&LinuxInfoLogImp::createTraceFsApi};
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen};
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readBackup{&NEO::SysCalls::sysCallsRead};
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup{&NEO::SysCalls::sysCallsClose};
    VariableBackup<decltype(NEO::SysCalls::sysCallsWrite)> writeBackup{&NEO::SysCalls::sysCallsWrite};
};

TEST_F(SysmanEventsInfoLogFixture, GivenEventFlagsWhichAreNotDriverScopedWhenRegisteringDriverEventsThenInvalidEnumerationErrorIsReturned) {
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED));
    EXPECT_EQ(0u, getDriverRegisteredEvents());
    EXPECT_TRUE(pEventsUtil->deviceEventsMap.empty());
}

TEST_F(SysmanEventsInfoLogFixture, GivenOsSysmanDriverIsNullWhenRegisteringDriverEventsThenUninitializedIsReturnedAndRegistrationIsNotUpdated) {
    VariableBackup<L0::Sysman::OsSysmanDriver *> osSysmanDriverBackup(&driverHandle->pOsSysmanDriver);
    driverHandle->pOsSysmanDriver = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(0u, getDriverRegisteredEvents());
    EXPECT_TRUE(pEventsUtil->deviceEventsMap.empty());
    EXPECT_EQ(0u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenDriverScopedEventsWhenRegisteringThemThenDeviceRegistrationsAreNotAffected) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
    EXPECT_TRUE(pEventsUtil->deviceEventsMap.empty());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_EQ(1u, static_cast<uint32_t>(pEventsUtil->deviceEventsMap.size()));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
}

TEST_F(SysmanEventsInfoLogFixture, GivenListenIsInFlightAndNoDriverScopedEventIsRegisteredWhenRegisteringDriverEventsThenPipeIsWritten) {
    startListenOnPipe();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
    EXPECT_EQ(1u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenListenIsInFlightAndDriverScopedEventIsRegisteredWhenClearingDriverEventsThenPipeIsWritten) {
    seedDriverScopedRegistration(ZES_INTEL_CPER_DATA_AVAILABLE);
    startListenOnPipe();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), 0));
    EXPECT_EQ(0u, getDriverRegisteredEvents());
    EXPECT_EQ(1u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenListenIsInFlightAndDriverScopedEventIsRegisteredWhenRegisteringTheSameDriverEventsAgainThenPipeIsNotWritten) {
    seedDriverScopedRegistration(ZES_INTEL_CPER_DATA_AVAILABLE);
    startListenOnPipe();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
    EXPECT_EQ(0u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenListenIsInFlightAndNoDriverScopedEventIsRegisteredWhenClearingDriverEventsThenPipeIsNotWritten) {
    startListenOnPipe();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), 0));
    EXPECT_EQ(0u, getDriverRegisteredEvents());
    EXPECT_EQ(0u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenNoListenIsInFlightWhenRegisteringDriverEventsThenRegistrationIsUpdatedAndPipeIsNotWritten) {
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
    EXPECT_EQ(0u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenNoListenIsInFlightAndDriverScopedEventIsRegisteredWhenRegisteringTheSameDriverEventsAgainThenPipeIsNotWritten) {
    seedDriverScopedRegistration(ZES_INTEL_CPER_DATA_AVAILABLE);
    EXPECT_EQ(-1, pEventsUtil->pipeFd[1]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
    EXPECT_EQ(0u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenWriteToPipeFailsWhenRegisteringDriverEventsThenSuccessIsReturnedAndRegistrationIsUpdated) {
    startListenOnPipe();
    writeReturnValue = -1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), getDriverRegisteredEvents());
    EXPECT_EQ(1u, writeCallCount);
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredAndInfoLogCollectionIsEnabledWhenListeningForDriverEventsThenEventIsReportedInDriverEventsAndRecordStaysReadable) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockInfoLogReadPipeFd;
        pipeFd[1] = mockInfoLogWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == MockTraceFsApiWithData::mockTracePipeFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[count] = {};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), driverEvents);

    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(hInfoLog, &size, buffer.data()));
    EXPECT_EQ(mockCperLen, size);
    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanEventsInfoLogFixture, GivenLibUdevIsNotAvailableAndCperDataAvailableIsRegisteredWhenListeningForDriverEventsThenCperEventIsStillReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockInfoLogReadPipeFd;
        pipeFd[1] = mockInfoLogWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == MockTraceFsApiWithData::mockTracePipeFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    pLinuxSysmanDriverImp->pUdevLib = nullptr;

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[count] = {};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), driverEvents);
    EXPECT_EQ(nullptr, pEventsUtil->pUdevLib);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredAndInfoLogCollectionIsNotEnabledWhenListeningForDriverEventsThenNoEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockInfoLogReadPipeFd;
        pipeFd[1] = mockInfoLogWritePipeFd;
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            pollFd[i].revents = POLLIN;
        }
        return static_cast<int>(numberOfFds);
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(-1, getCperTracePipeFd());

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[count] = {};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 0u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsInfoLogFixture, GivenGarbageValueInDriverEventsWhenNoDriverScopedEventOccursThenDriverEventsIsCleared) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return 0;
    });

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 5u;
    zes_event_type_flags_t pDeviceEvents[count] = {driverSlotSentinel};
    zes_event_type_flags_t driverEvents = driverSlotSentinel;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 0u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhenCallingListenSystemEventsWithDriverEventsThenOnlyDriverEventsIsUpdated) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, [](int pipeFd[2]) -> int {
        pipeFd[0] = mockInfoLogReadPipeFd;
        pipeFd[1] = mockInfoLogWritePipeFd;
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        for (uint64_t i = 0; i < numberOfFds; i++) {
            if (pollFd[i].fd == MockTraceFsApiWithData::mockTracePipeFd) {
                pollFd[i].revents = POLLIN;
            }
        }
        return 1;
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    seedDriverScopedRegistration(ZES_INTEL_CPER_DATA_AVAILABLE);

    constexpr uint32_t count = 1u;
    std::vector<zes_event_type_flags_t> registeredEvents(count, 0);
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pEvents[count] = {0};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_TRUE(pEventsUtil->listenSystemEvents(pEvents, count, registeredEvents, phDevices, 0u, &driverEvents));
    EXPECT_EQ(0u, pEvents[0]);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), driverEvents);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenInfoLogCollectionIsEnabledAndOnlyDeviceScopedEventIsRegisteredWhenListeningForEventsThenTracefsSourceIsNotAdded) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));

    zes_device_handle_t phDevices[1] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1000u, 1u, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(1u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhenListeningWithZesDriverEventListenExThenTracefsSourceIsNotAddedAndOnlyDeviceSlotsAreWritten) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[count + 1] = {0, driverSlotSentinel};
    uint32_t numDeviceEvents = count;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(driverSlotSentinel, pDeviceEvents[count]);

    EXPECT_EQ(1u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhenListeningWithZesDriverEventListenThenTracefsSourceIsNotAddedAndOnlyDeviceSlotsAreWritten) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[count + 1] = {0, driverSlotSentinel};
    uint32_t numDeviceEvents = count;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListen(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(driverSlotSentinel, pDeviceEvents[count]);

    EXPECT_EQ(1u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhenListeningWithZesDriverEventListenExAndNumDeviceEventsSetToCountPlusOneThenDriverSlotIsUntouched) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[count + 1] = {0, driverSlotSentinel};
    uint32_t numDeviceEvents = count + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(driverSlotSentinel, pDeviceEvents[count]);

    EXPECT_EQ(1u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhenListeningWithNullDriverEventsThenTracefsSourceIsNotAdded) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[count] = {0};
    uint32_t numDeviceEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, nullptr));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);

    EXPECT_EQ(1u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenTracefsPollSourceIsReadyAndDriverEventsOutputIsNullWhenListeningForEventsThenNoDriverScopedEventIsReported) {
    auto pMockEventsUtil = std::make_unique<MockLinuxEventsUtilWithUnrequestedTracefsSource>(pLinuxSysmanDriverImp.get());
    VariableBackup<L0::Sysman::LinuxEventsUtil *> eventsUtilBackup(&pLinuxSysmanDriverImp->pLinuxEventsUtil, pMockEventsUtil.get());

    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, mockTracefsFd);
    });

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), pMockEventsUtil->registeredDriverEvents);

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[count] = {0};
    uint32_t numDeviceEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverEventListenEx(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);

    EXPECT_EQ(1u, pMockEventsUtil->updateCperPollSourceCallCount);
    EXPECT_FALSE(pMockEventsUtil->cperRegisteredAfterUpdate);
    EXPECT_EQ(1u, pollCallCount);
    EXPECT_TRUE(wasFdPolled(0, mockTracefsFd));
}

TEST_F(SysmanEventsInfoLogFixture, GivenDeviceScopedAndDriverScopedEventsOccurInTheSameWakeUpWhenListeningForDriverEventsThenBothAreReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        return markFdReady(pollFd, numberOfFds, mockUdevFd) +
               markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto pSysfsAccess = std::make_unique<MockEventsSysfsAccess>();
    auto *pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    pUdevLib->getEventTypeResult = "remove";

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEventRegister(device->toHandle(), ZES_EVENT_TYPE_FLAG_DEVICE_DETACH));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    zes_event_type_flags_t pDeviceEvents[count] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(1u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH), pDeviceEvents[0]);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), driverEvents);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhileListeningWhenRegistrationPipeIsNotifiedThenTracefsSourceIsAddedAndEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            pEventsUtilForPoll->driverEventRegister(ZES_INTEL_CPER_DATA_AVAILABLE);
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd);
        }
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_EQ(0u, getDriverRegisteredEvents());

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, pDeviceEvents[0]);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), driverEvents);

    EXPECT_EQ(2u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));
    EXPECT_TRUE(wasFdPolled(1, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(1u, pipeReadCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsRegisteredWhileListeningAndInfoLogCollectionIsNotEnabledWhenRegistrationPipeIsNotifiedThenNoTracefsSourceIsAddedAndNoEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            pEventsUtilForPoll->driverEventRegister(ZES_INTEL_CPER_DATA_AVAILABLE);
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd);
        }
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    EXPECT_EQ(-1, getCperTracePipeFd());

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);

    EXPECT_EQ(2u, pollCallCount);
    EXPECT_FALSE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));
    EXPECT_FALSE(wasFdPolled(1, MockTraceFsApiWithData::mockTracePipeFd));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsUnregisteredWhileListeningWhenRegistrationPipeIsNotifiedThenListenReturnsWithoutReportingTheEvent) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            pEventsUtilForPoll->driverEventRegister(0);
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd);
        }
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);

    EXPECT_EQ(1u, pollCallCount);
    EXPECT_TRUE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenDriverScopedRegistrationIsDroppedWhileListeningWhenRegistrationPipeIsNotifiedThenTracefsSourceIsRemovedAndNoEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            dropDriverScopedRegistrationFromPoll();
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd);
        }
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);
    EXPECT_EQ(1u, pollCallCount);
    EXPECT_TRUE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenCperDataAvailableIsUnregisteredInTheSameWakeUpWhichCarriesTracePipeDataWhenListeningForEventsThenTheEventIsNotReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            pEventsUtilForPoll->driverEventRegister(0);
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd) +
                   markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
        }
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);

    EXPECT_EQ(1u, pollCallCount);
    EXPECT_TRUE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, false));
}

TEST_F(SysmanEventsInfoLogFixture, GivenInfoLogCollectionIsDisabledWhileListeningWhenRegistrationPipeIsNotifiedThenTracefsSourceIsRemovedAndNoEventIsReported) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            pDriverImpForPoll->setCperTracePipeFd(-1);
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd);
        }
        return markFdReady(pollFd, numberOfFds, MockTraceFsApiWithData::mockTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(0u, driverEvents);

    EXPECT_EQ(2u, pollCallCount);
    EXPECT_TRUE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));
    EXPECT_FALSE(wasFdPolled(1, MockTraceFsApiWithData::mockTracePipeFd));
}

TEST_F(SysmanEventsInfoLogFixture, GivenTracePipeIsReopenedWhileListeningWhenRegistrationPipeIsNotifiedThenTracefsSourceUsesTheNewDescriptor) {
    VariableBackup<decltype(SysCalls::sysCallsPipe)> mockPipe(&SysCalls::sysCallsPipe, mockSysCallsPipe);
    VariableBackup<decltype(SysCalls::sysCallsPoll)> mockPoll(&SysCalls::sysCallsPoll, [](struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) -> int {
        recordPollCall(pollFd, numberOfFds);
        if (pollCallCount == 1u) {
            pDriverImpForPoll->setCperTracePipeFd(mockReopenedTracePipeFd);
            return markFdReady(pollFd, numberOfFds, mockInfoLogReadPipeFd);
        }
        return markFdReady(pollFd, numberOfFds, mockReopenedTracePipeFd);
    });

    auto hInfoLog = getInfoLogHandle();
    ASSERT_NE(nullptr, hInfoLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, true));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));

    constexpr uint32_t count = 0u;
    zes_device_handle_t *phDevices = nullptr;
    zes_event_type_flags_t pDeviceEvents[1] = {0};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEventListenExp(driverHandle->toHandle(), 1000u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, numDeviceEvents);
    EXPECT_EQ(static_cast<zes_event_type_flags_t>(ZES_INTEL_CPER_DATA_AVAILABLE), driverEvents);

    EXPECT_EQ(2u, pollCallCount);
    EXPECT_TRUE(wasFdPolled(0, MockTraceFsApiWithData::mockTracePipeFd));
    EXPECT_FALSE(wasFdPolled(1, MockTraceFsApiWithData::mockTracePipeFd));
    EXPECT_TRUE(wasFdPolled(1, mockReopenedTracePipeFd));
}

TEST_F(SysmanEventsInfoLogFixture, GivenOsSysmanDriverIsNullWhenListeningForDriverEventsThenUninitializedIsReturned) {
    VariableBackup<L0::Sysman::OsSysmanDriver *> osSysmanDriverBackup(&driverHandle->pOsSysmanDriver);
    driverHandle->pOsSysmanDriver = nullptr;

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[count] = {};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEventListenExp(driverHandle->toHandle(), 0u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
    EXPECT_EQ(0u, driverEvents);
}

TEST_F(SysmanEventsInfoLogFixture, GivenSysmanInitFromCoreWhenCallingDriverEventRegisterEntrypointThenUnsupportedFeatureIsReturned) {
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, true);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(0u, getDriverRegisteredEvents());
}

TEST_F(SysmanEventsInfoLogFixture, GivenNeitherInitFlagSetWhenCallingDriverEventRegisterEntrypointThenUninitializedIsReturned) {
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, false);
    VariableBackup<bool> sysmanOnlyInitBackup(&L0::Sysman::sysmanOnlyInit, false);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEventRegister(driverHandle->toHandle(), ZES_INTEL_CPER_DATA_AVAILABLE));
    EXPECT_EQ(0u, getDriverRegisteredEvents());
}

TEST_F(SysmanEventsInfoLogFixture, GivenSysmanInitFromCoreWhenCallingDriverEventListenEntrypointThenUnsupportedFeatureIsReturned) {
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, true);

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[count] = {};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDriverEventListenExp(driverHandle->toHandle(), 0u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
}

TEST_F(SysmanEventsInfoLogFixture, GivenNeitherInitFlagSetWhenCallingDriverEventListenEntrypointThenUninitializedIsReturned) {
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, false);
    VariableBackup<bool> sysmanOnlyInitBackup(&L0::Sysman::sysmanOnlyInit, false);

    constexpr uint32_t count = 1u;
    zes_device_handle_t phDevices[count] = {device->toHandle()};
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t pDeviceEvents[count] = {};
    zes_event_type_flags_t driverEvents = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEventListenExp(driverHandle->toHandle(), 0u, count, phDevices, &numDeviceEvents, pDeviceEvents, &driverEvents));
}

TEST_F(SysmanEventsInfoLogFixture, GivenTracePipeIsOpenAndInfoLogHandlesWereNotEnumeratedWhenDestroyingTheDriverThenTracePipeIsClosed) {
    MockTraceFsApiWithData::closeCallCount = 0;
    {
        auto pDriverImp = std::make_unique<PublicLinuxSysmanDriverImp>();
        pDriverImp->setCperTracePipeFd(MockTraceFsApiWithData::mockTracePipeFd);
    }
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
