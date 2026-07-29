/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/info_log/linux/mock_sysman_info_log.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t handleCount = 1u;

class SysmanInfoLogFixture : public ::testing::Test {
  protected:
    void SetUp() override {
        allowFakeDevicePathBackup = true;
        loadFuncBackup = mockLoadFunc;

        VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
            constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
            strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
            return buf;
        });

        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });

        execEnv = new NEO::ExecutionEnvironment();
        execEnv->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            execEnv->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            execEnv->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*execEnv->rootDeviceEnvironments[i]));
        }

        ze_result_t returnValue;
        driverHandle = L0::Sysman::SysmanDriverHandle::create(*execEnv, &returnValue);
        L0::Sysman::sysmanOnlyInit = true;
    }

    void TearDown() override {
        if (driverHandle) {
            delete driverHandle;
            driverHandle = nullptr;
        }
        L0::Sysman::globalSysmanDriver = nullptr;
        L0::Sysman::sysmanOnlyInit = false;
    }

    std::vector<zes_intel_info_log_handle_t> getInfoLogHandles(uint32_t count) {
        std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    static NEO::OsLibrary *mockLoadFunc(const NEO::OsLibraryCreateProperties &) {
        return new MockTraceFsOsLibrary();
    }

    L0::Sysman::SysmanDriverHandle *driverHandle = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    const uint32_t numRootDevices = 1u;
    VariableBackup<bool> allowFakeDevicePathBackup{&NEO::SysCalls::allowFakeDevicePath};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup{&NEO::OsLibrary::loadFunc};
};

TEST_F(SysmanInfoLogFixture, GivenDefaultCreateTraceFsApiLambdaWhenInvokedThenNonNullTraceFsApiIsReturned) {
    auto traceFsApi = LinuxInfoLogImp::createTraceFsApi();
    EXPECT_NE(nullptr, traceFsApi);
}

TEST_F(SysmanInfoLogFixture, GivenTracepointPathInaccessibleWhenEnumeratingInfoLogsThenNoHandlesAreReturned) {
    VariableBackup<bool> failAccessBackup(&NEO::SysCalls::failAccess, true);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanInfoLogFixture, GivenValidDriverHandleWhenEnumeratingInfoLogsThenSuccessAndOneCountAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanInfoLogFixture, GivenDriverWhenEnumeratingInfoLogsThenHandleCreationSucceeds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));

    auto hInfoLog = handles[0];
    EXPECT_NE(nullptr, hInfoLog);
}

TEST_F(SysmanInfoLogFixture, GivenRequestedInfoLogCountGreaterThanOneWhenEnumeratingInfoLogsThenOneHandleCountIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);

    count = count + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(handleCount, count);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingGetPropertiesApiThenValidPropertiesAndSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));

    EXPECT_EQ(ZES_INTEL_INFO_LOG_TYPE_EXP_DEVICE, properties.infoLogType);
    EXPECT_EQ(ZES_INTEL_INFO_LOG_FORMAT_CPER, properties.infoLogFormat);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize), properties.maxSize);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsBufferSizeReturnsNegativeOneWhenGettingPropertiesThenMaxSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<PublicTraceFsApi>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));

    EXPECT_EQ(0u, properties.maxSize);
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnumeratingInfoLogsThenErrorIsReturned) {
    auto *pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    auto *originalOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pSysmanDriverHandleImp->enumInfoLogs(&count, nullptr));

    pSysmanDriverHandleImp->pOsSysmanDriver = originalOsSysmanDriver;
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenEnablingInfoLogSuccessfullyThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], true));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenDisablingInfoLogSuccessfullyThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], false));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenTraceFsEventEnableFailsThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->eventEnableReturnValue = -1;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], true));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenTraceFsTraceOnFailsThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->eventEnableReturnValue = 0;
        mockApi->traceOnReturnValue = -1;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], true));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenTraceFsEventDisableFailsThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->eventDisableReturnValue = -1;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], false));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenTraceFsTraceOffFailsThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->eventDisableReturnValue = 0;
        mockApi->traceOffReturnValue = -1;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], false));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenReadingValidCperDataThenSuccessAndDataIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(mockCperLen, size);

    // Verify first bytes of CPER record
    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }
}

TEST_F(SysmanInfoLogFixture, GivenFirstTraceFsPathFailsWhenReadingInfoLogThenFallsBackToSecondPathAndSucceeds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(false, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(mockCperLen, size);

    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenBufferSizeIsZeroThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t zeroSize = 0;
    std::vector<uint8_t> buffer(1);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogReadExp(infoLogHandles[0], &zeroSize, buffer.data()));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenReadingMultipleCperDataThenSuccessAndAllDataIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Total size is 3 * 532 = 1596 bytes
    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(mockCperLen * 3u, size);

    // Verify first CPER record bytes
    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }

    // Verify second CPER record bytes (starts at offset 532)
    for (uint32_t i = 0; i < expectedCper2Bytes.size(); i++) {
        EXPECT_EQ(expectedCper2Bytes[i], buffer[mockCperLen + i]);
    }

    // Verify third CPER record bytes (starts at offset 1064)
    for (uint32_t i = 0; i < expectedCper3Bytes.size(); i++) {
        EXPECT_EQ(expectedCper3Bytes[i], buffer[mockCperLen * 2 + i]);
    }

    // Verify record_id byte differs between events
    EXPECT_EQ(0x06, buffer[cperRecordIdOffset]);
    EXPECT_EQ(0x07, buffer[mockCperLen + cperRecordIdOffset]);
    EXPECT_EQ(0x08, buffer[mockCperLen * 2 + cperRecordIdOffset]);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenBufferCanFitOnlyOneCperThenOnlyOneCperIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true); // 3 CPER records available
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Allocate buffer that can fit only 1 CPER (532 bytes) but not 2 (1064 bytes)
    uint32_t size = 600u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));

    // Only 1 CPER (532 bytes) fits in 600-byte buffer
    EXPECT_EQ(mockCperLen, size);

    // Verify first CPER record bytes
    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }

    // Verify distinguishing byte for first CPER record
    EXPECT_EQ(0x06, buffer[cperRecordIdOffset]);
}

TEST_F(SysmanInfoLogFixture, GivenCorruptedOddLengthCperRawWhenReadingInfoLogThenZeroBytesAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCorruptedCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInvalidHexCharacterInCperRawWhenReadingInfoLogThenZeroBytesAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockInvalidHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenMissingCperLenFieldWhenReadingInfoLogThenZeroBytesAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockMissingFieldCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCompactHexCperRawWithNoSpacesWhenReadingInfoLogThenDataIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCompactHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenSpacedHexCperRawWhenReadingInfoLogThenDataIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSpacedHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenCperRawWithMultipleSpacedBytesAndTrailingFieldWhenReadingInfoLogThenDataIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithTrailingField);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(4u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
    EXPECT_EQ(0xEF, buffer[2]);
    EXPECT_EQ(0x01, buffer[3]);
}

TEST_F(SysmanInfoLogFixture, GivenTraceOutputWithNonCperLineWhenReadingInfoLogThenNonCperLineIsSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithHeaderLine);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenCperEventWithZeroLengthWhenReadingInfoLogThenRecordIsSkippedAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithZeroLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeOpenFailsWhenReadingInfoLogThenSizeIsZeroAndErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeReturnsZeroLenCperWhenReadingInfoLogThenCperLenIsZeroAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData, false, &mockCperEventWithZeroLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenReadFromTracePipeReturnsErrorWhenReadingInfoLogThenBytesReadIsNegativeAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsReadError);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperRawWithEmptyValueWhenReadingInfoLogThenWhileConditionIsFalseAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithEmptyCperRaw);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperLenFieldEmptyAtEndOfLineWhenReadingInfoLogThenWhileConditionIsFalseAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithEmptyCperLen);
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperRawByteCountMismatchesLenFieldWhenReadingInfoLogThenCperDataSizeNotEqualCperLenAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithMismatchedLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenNullBufferWhenReadingInfoLogThenPBufferIsNullAndErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogReadExp(infoLogHandles[0], &size, nullptr));
}

TEST_F(SysmanInfoLogFixture, GivenNullSizePointerWhenReadingInfoLogThenPSizeIsNullAndErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    std::vector<uint8_t> buffer(1024u);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogReadExp(infoLogHandles[0], nullptr, buffer.data()));
}

TEST_F(SysmanInfoLogFixture, GivenTraceFileReadReturnsNullWhenReadingInfoLogThenTraceDataIsNullAndErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(MockTraceFsApiWithBadCperData::emptyStr, false, nullptr, true);
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenReadReturnsEagainWhenReadingInfoLogThenReadStopsAndZeroBytesAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCompactHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsReadWithInitialEagain);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeReturnsLargerCperThanTracePredictedWhenReadingInfoLogThenWarningDroppedDataIsReturnedWithNoData) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSmallCperTraceEvent, false, &mockLargerCperTracePipeEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Buffer is 3 bytes: fits the 2-byte record seen in 'trace', but not the 4-byte record in 'trace_pipe'
    uint32_t size = 3u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeDeliversExtraLargerRecordAfterFittingRecordsWhenReadingInfoLogThenWarningDroppedDataIsReturnedWithPartialData) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockTwoSmallCperTraceEvents, false, &mockTwoSmallPlusOneLargerCperTracePipeEvents);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Buffer fits 3x 2-byte records (6 bytes) but not 2x 2-byte + 1x 4-byte (8 bytes)
    uint32_t size = 7u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    // First 2 records (4 bytes) were written before the 3rd record overflowed
    EXPECT_EQ(4u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
    EXPECT_EQ(0xEF, buffer[2]);
    EXPECT_EQ(0x01, buffer[3]);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
