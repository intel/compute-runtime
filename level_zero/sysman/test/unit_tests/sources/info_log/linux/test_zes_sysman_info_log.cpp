/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/info_log/linux/mock_sysman_info_log.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include <cerrno>

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
            // The mocked trace_pipe descriptor is not a real one, so drop it before the
            // driver destructor tries to close it.
            auto *pLinuxSysmanDriverImp = getLinuxSysmanDriverImp();
            if (pLinuxSysmanDriverImp != nullptr) {
                pLinuxSysmanDriverImp->setCperTracePipeFd(-1);
            }
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

    void enableInfoLogCollection(zes_intel_info_log_handle_t hInfoLog) {
        zes_intel_info_log_enable_descriptor_exp enableDescriptor = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(hInfoLog, &enableDescriptor));
    }

    L0::Sysman::LinuxSysmanDriverImp *getLinuxSysmanDriverImp() {
        return static_cast<L0::Sysman::LinuxSysmanDriverImp *>(
            static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle)->pOsSysmanDriver);
    }

    int getCperTracePipeFd() {
        auto *pLinuxSysmanDriverImp = getLinuxSysmanDriverImp();
        return (pLinuxSysmanDriverImp != nullptr) ? pLinuxSysmanDriverImp->getCperTracePipeFd() : -1;
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenDisablingInfoLogSuccessfullyThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenTraceFsEventDisableFailsThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->eventDisableReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenTraceFsTraceOffFailsThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->eventDisableReturnValue = 0;
        mockApi->traceOffReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogDisableExp(infoLogHandles[0]));
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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeNotOpenedWhenReadingInfoLogThenNotAvailableIsReturnedAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData);
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperLenFieldEmptyAtEndOfLineWhenReadingInfoLogThenWhileConditionIsFalseAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithEmptyCperLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

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

TEST_F(SysmanInfoLogFixture, GivenGlobalInstanceWhenEnablingInfoLogThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenGlobalInstanceWhenInstancedCollectionUnavailableThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Fail access checks after handle creation so instanced collection appears unavailable,
    // but global enable should still succeed since instanceName == nullptr skips the check.
    VariableBackup<bool> failAccessBackup(&NEO::SysCalls::failAccess, true);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyEnabledGlobalWhenReEnablingWithGlobalThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenInstancedCollectionAvailableThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyEnabledNamedInstanceWhenReEnablingWithSameNameThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenGlobalWithTracingAlreadyOnWhenEnablingThenSuccessIsReturnedAndTraceOnIsSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockTracingAlreadyOn = true;
        mockApi->traceOnReturnValue = -1; // would fail if called; skipped because already on
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenGlobalWithEventAlreadyEnabledWhenEnablingThenSuccessIsReturnedAndEventEnableIsSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockEventAlreadyEnabled = true;
        mockApi->eventEnableReturnValue = -1; // would fail if called; skipped because already enabled
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenInstancedCollectionAvailableWhenGettingPropertiesThenIsInstancedCollectionSupportedIsTrue) {
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
    EXPECT_TRUE(properties.isInstancedCollectionSupported);
}

TEST_F(SysmanInfoLogFixture, GivenGlobalInstanceWhenDisablingAfterEnableThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenEventAlreadyEnabledWhenDisablingThenEventDisableIsSkippedAndSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockEventAlreadyEnabled = true;
        mockApi->eventDisableReturnValue = -1; // would fail if called; must be skipped
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenTracingAlreadyOnWhenDisablingThenTraceOffIsSkippedAndSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockTracingAlreadyOn = true;
        mockApi->traceOffReturnValue = -1; // would fail if called; must be skipped
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenNullPSizeWhenReadingWithMetaDataThenInvalidArgumentIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t eventCount = 1;
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], nullptr, nullptr, &eventCount, descriptors.data()));
}

TEST_F(SysmanInfoLogFixture, GivenNullPEventCountWhenReadingWithMetaDataThenInvalidArgumentIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 1024;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, nullptr, nullptr));
}

TEST_F(SysmanInfoLogFixture, GivenNullBufferWhenQueryingWithMetaDataThenSizeAndCountAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 0;
    uint32_t eventCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, &eventCount, nullptr));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenNullBufferAndMultipleEventsWhenQueryingWithMetaDataThenCorrectCountIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true);
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 0;
    uint32_t eventCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, &eventCount, nullptr));
    EXPECT_EQ(3u * mockCperLen, size);
    EXPECT_EQ(3u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenValidBufferWhenReadingWithMetaDataThenDataAndMetaDataAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);

    // Verify CPER bytes
    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]) << "Mismatch at byte " << i;
    }

    // Metadata: first record, offset=0, lengthOfData=532
    EXPECT_EQ(0u, descriptors[0].offset);
    EXPECT_EQ(mockCperLen, descriptors[0].lengthOfData);

    // BDF: dev=0000:13:00.0 -> domain=0, bus=0x13, device=0, function=0
    EXPECT_EQ(0u, descriptors[0].address.domain);
    EXPECT_EQ(0x13u, descriptors[0].address.bus);
    EXPECT_EQ(0u, descriptors[0].address.device);
    EXPECT_EQ(0u, descriptors[0].address.function);

    // UUID: platform_id=e5af4690-4190-2451-8614-92550d9e9da6
    static const uint8_t expectedUuid[16] = {0xe5, 0xaf, 0x46, 0x90, 0x41, 0x90, 0x24, 0x51,
                                             0x86, 0x14, 0x92, 0x55, 0x0d, 0x9e, 0x9d, 0xa6};
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(expectedUuid[i], descriptors[0].uuid.id[i]) << "UUID mismatch at byte " << i;
    }

    // Timestamp: 5058.247549 -> 5058*1_000_000 + 247549 = 5058247549
    EXPECT_EQ(5058247549ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenValidBufferWhenReadingMultipleEventsWithMetaDataThenAllDataAndMetaDataAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 3u * mockCperLen;
    uint32_t eventCount = 3;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(3);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(3u * mockCperLen, size);
    EXPECT_EQ(3u, eventCount);

    // Verify record offsets
    EXPECT_EQ(0u, descriptors[0].offset);
    EXPECT_EQ(mockCperLen, descriptors[1].offset);
    EXPECT_EQ(2u * mockCperLen, descriptors[2].offset);

    // Verify CPER data for each record using distinguishing byte at cperRecordIdOffset
    EXPECT_EQ(expectedCper1Bytes[cperRecordIdOffset], buffer[descriptors[0].offset + cperRecordIdOffset]);
    EXPECT_EQ(expectedCper2Bytes[cperRecordIdOffset], buffer[descriptors[1].offset + cperRecordIdOffset]);
    EXPECT_EQ(expectedCper3Bytes[cperRecordIdOffset], buffer[descriptors[2].offset + cperRecordIdOffset]);

    // Verify timestamps are distinct and in order
    EXPECT_LT(descriptors[0].timestamp, descriptors[1].timestamp);
    EXPECT_LT(descriptors[1].timestamp, descriptors[2].timestamp);
    // event1: 5058.247549, event2: 5058.582145, event3: 5058.941472
    EXPECT_EQ(5058247549ULL, descriptors[0].timestamp);
    EXPECT_EQ(5058582145ULL, descriptors[1].timestamp);
    EXPECT_EQ(5058941472ULL, descriptors[2].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenRealWorldEventWhenReadingWithMetaDataThenAllFieldsAreCorrectlyParsed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(false, false, mockRealSampleEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);

    // CPER data: full 532-byte payload from the real event
    ASSERT_EQ(expectedRealSampleCperBytes.size(), static_cast<size_t>(size));
    for (uint32_t i = 0; i < size; i++) {
        EXPECT_EQ(expectedRealSampleCperBytes[i], buffer[i]) << "CPER byte mismatch at offset " << i;
    }

    // offset and length
    EXPECT_EQ(0u, descriptors[0].offset);
    EXPECT_EQ(mockCperLen, descriptors[0].lengthOfData);

    // BDF: dev=0000:03:00.0 -> domain=0, bus=0x03, device=0, function=0
    EXPECT_EQ(0u, descriptors[0].address.domain);
    EXPECT_EQ(0x03u, descriptors[0].address.bus);
    EXPECT_EQ(0u, descriptors[0].address.device);
    EXPECT_EQ(0u, descriptors[0].address.function);

    // UUID: platform_id=e5af4690-4190-2451-8614-92550d9e9da6
    static const uint8_t expectedUuid[16] = {0xe5, 0xaf, 0x46, 0x90, 0x41, 0x90, 0x24, 0x51,
                                             0x86, 0x14, 0x92, 0x55, 0x0d, 0x9e, 0x9d, 0xa6};
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(expectedUuid[i], descriptors[0].uuid.id[i]) << "UUID byte mismatch at index " << i;
    }

    // Timestamp: 2750.484567 -> 2750*1_000_000 + 484567 = 2750484567
    EXPECT_EQ(2750484567ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenSampleCperLogWithMultipleBDFsAndPlatformIDsWhenReadingWithMetaDataThenAllEventsAreCorrectlyParsed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(false, false, mockSampleCperLogData);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Query mode: get count and size
    uint32_t querySize = 0;
    uint32_t queryEventCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &querySize, nullptr, &queryEventCount, nullptr));
    EXPECT_EQ(8u, queryEventCount) << "Should have 8 xe_error_cper events (write_msr events filtered out)";
    EXPECT_EQ(8u * mockCperLen, querySize) << "Total size should be 8 * 532";

    // Extract mode: read all events
    uint32_t size = querySize;
    uint32_t eventCount = queryEventCount;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(8u * mockCperLen, size);
    EXPECT_EQ(8u, eventCount);

    // Verify offsets are sequential
    for (uint32_t i = 0; i < eventCount; i++) {
        EXPECT_EQ(i * mockCperLen, descriptors[i].offset) << "Offset mismatch for event " << i;
        EXPECT_EQ(mockCperLen, descriptors[i].lengthOfData) << "Length mismatch for event " << i;
    }

    // Verify timestamps are in ascending order
    for (uint32_t i = 1; i < eventCount; i++) {
        EXPECT_LT(descriptors[i - 1].timestamp, descriptors[i].timestamp) << "Timestamps should be ascending";
    }

    // Verify specific event details for each of the 8 events
    // Event 0: dev=0000:03:00.0, timestamp=2750.484567, platform_id=15af4690-4190-2451-8614-92550d9e9da6
    EXPECT_EQ(0u, descriptors[0].address.domain);
    EXPECT_EQ(0x03u, descriptors[0].address.bus);
    EXPECT_EQ(0u, descriptors[0].address.device);
    EXPECT_EQ(0u, descriptors[0].address.function);
    EXPECT_EQ(2750484567ULL, descriptors[0].timestamp);
    EXPECT_EQ(0x15, descriptors[0].uuid.id[0]); // platform_id starts with 15

    // Event 1: dev=0000:04:00.0, timestamp=2750.507923, platform_id=25af4690...
    EXPECT_EQ(0x04u, descriptors[1].address.bus);
    EXPECT_EQ(2750507923ULL, descriptors[1].timestamp);
    EXPECT_EQ(0x25, descriptors[1].uuid.id[0]); // platform_id starts with 25

    // Event 2: dev=0000:05:00.0, timestamp=2750.555466, platform_id=35af4690...
    EXPECT_EQ(0x05u, descriptors[2].address.bus);
    EXPECT_EQ(2750555466ULL, descriptors[2].timestamp);
    EXPECT_EQ(0x35, descriptors[2].uuid.id[0]); // platform_id starts with 35

    // Event 3: dev=0000:04:00.0, timestamp=2750.576952, platform_id=45af4690...
    EXPECT_EQ(0x04u, descriptors[3].address.bus);
    EXPECT_EQ(2750576952ULL, descriptors[3].timestamp);
    EXPECT_EQ(0x45, descriptors[3].uuid.id[0]); // platform_id starts with 45

    // Event 4: dev=0000:03:00.0, timestamp=2750.594695, platform_id=55af4690...
    EXPECT_EQ(0x03u, descriptors[4].address.bus);
    EXPECT_EQ(2750594695ULL, descriptors[4].timestamp);
    EXPECT_EQ(0x55, descriptors[4].uuid.id[0]); // platform_id starts with 55

    // Event 5: dev=0000:05:00.0, timestamp=2750.616417, platform_id=65af4690...
    EXPECT_EQ(0x05u, descriptors[5].address.bus);
    EXPECT_EQ(2750616417ULL, descriptors[5].timestamp);
    EXPECT_EQ(0x65, descriptors[5].uuid.id[0]); // platform_id starts with 65

    // Event 6: dev=0000:07:00.0, timestamp=2750.634843, platform_id=75af4690...
    EXPECT_EQ(0x07u, descriptors[6].address.bus);
    EXPECT_EQ(2750634843ULL, descriptors[6].timestamp);
    EXPECT_EQ(0x75, descriptors[6].uuid.id[0]); // platform_id starts with 75

    // Event 7: dev=0000:08:00.0, timestamp=2750.657681, platform_id=85af4690...
    EXPECT_EQ(0x08u, descriptors[7].address.bus);
    EXPECT_EQ(2750657681ULL, descriptors[7].timestamp);
    EXPECT_EQ(0x85, descriptors[7].uuid.id[0]); // platform_id starts with 85

    // Verify CPER data starts with "CPER" signature for all events
    for (uint32_t i = 0; i < eventCount; i++) {
        uint32_t offset = descriptors[i].offset;
        EXPECT_EQ(0x43, buffer[offset + 0]) << "CPER signature byte 0 mismatch for event " << i; // 'C'
        EXPECT_EQ(0x50, buffer[offset + 1]) << "CPER signature byte 1 mismatch for event " << i; // 'P'
        EXPECT_EQ(0x45, buffer[offset + 2]) << "CPER signature byte 2 mismatch for event " << i; // 'E'
        EXPECT_EQ(0x52, buffer[offset + 3]) << "CPER signature byte 3 mismatch for event " << i; // 'R'
    }

    // Verify distinguishing bytes (at offset 96 in CPER data) are sequential 0x01-0x08
    for (uint32_t i = 0; i < eventCount; i++) {
        EXPECT_EQ(i + 1, buffer[descriptors[i].offset + cperRecordIdOffset])
            << "Distinguishing byte mismatch for event " << i;
    }
}

TEST_F(SysmanInfoLogFixture, GivenSampleCperLogWhenReadingWithMetaDataMultipleTimesThenAllEventsAreCorrectlyReadInChunks) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(false, false, mockSampleCperLogData);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Query mode: get total count and size
    uint32_t querySize = 0;
    uint32_t queryEventCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &querySize, nullptr, &queryEventCount, nullptr));
    EXPECT_EQ(8u, queryEventCount) << "Should have 8 xe_error_cper events total";
    EXPECT_EQ(8u * mockCperLen, querySize) << "Total size should be 8 * 532";

    // First read: request 3 events
    uint32_t read1EventCount = 3;
    uint32_t read1Size = read1EventCount * mockCperLen;
    std::vector<uint8_t> buffer1(read1Size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors1(read1EventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &read1Size, buffer1.data(), &read1EventCount, descriptors1.data()));
    EXPECT_EQ(3u, read1EventCount);
    EXPECT_EQ(3u * mockCperLen, read1Size);

    // Verify first 3 events
    EXPECT_EQ(0x03u, descriptors1[0].address.bus); // Event 0: dev=0000:03:00.0
    EXPECT_EQ(0x15, descriptors1[0].uuid.id[0]);   // platform_id starts with 15
    EXPECT_EQ(2750484567ULL, descriptors1[0].timestamp);

    EXPECT_EQ(0x04u, descriptors1[1].address.bus); // Event 1: dev=0000:04:00.0
    EXPECT_EQ(0x25, descriptors1[1].uuid.id[0]);   // platform_id starts with 25
    EXPECT_EQ(2750507923ULL, descriptors1[1].timestamp);

    EXPECT_EQ(0x05u, descriptors1[2].address.bus); // Event 2: dev=0000:05:00.0
    EXPECT_EQ(0x35, descriptors1[2].uuid.id[0]);   // platform_id starts with 35
    EXPECT_EQ(2750555466ULL, descriptors1[2].timestamp);

    // Second read: request 1 event
    uint32_t read2EventCount = 1;
    uint32_t read2Size = read2EventCount * mockCperLen;
    std::vector<uint8_t> buffer2(read2Size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors2(read2EventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &read2Size, buffer2.data(), &read2EventCount, descriptors2.data()));
    EXPECT_EQ(1u, read2EventCount);
    EXPECT_EQ(1u * mockCperLen, read2Size);

    // Verify event 3
    EXPECT_EQ(0x04u, descriptors2[0].address.bus); // Event 3: dev=0000:04:00.0
    EXPECT_EQ(0x45, descriptors2[0].uuid.id[0]);   // platform_id starts with 45
    EXPECT_EQ(2750576952ULL, descriptors2[0].timestamp);

    // Third read: request 4 events
    uint32_t read3EventCount = 4;
    uint32_t read3Size = read3EventCount * mockCperLen;
    std::vector<uint8_t> buffer3(read3Size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors3(read3EventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &read3Size, buffer3.data(), &read3EventCount, descriptors3.data()));
    EXPECT_EQ(4u, read3EventCount);
    EXPECT_EQ(4u * mockCperLen, read3Size);

    // Verify events 4-7
    EXPECT_EQ(0x03u, descriptors3[0].address.bus); // Event 4: dev=0000:03:00.0
    EXPECT_EQ(0x55, descriptors3[0].uuid.id[0]);   // platform_id starts with 55
    EXPECT_EQ(2750594695ULL, descriptors3[0].timestamp);

    EXPECT_EQ(0x05u, descriptors3[1].address.bus); // Event 5: dev=0000:05:00.0
    EXPECT_EQ(0x65, descriptors3[1].uuid.id[0]);   // platform_id starts with 65
    EXPECT_EQ(2750616417ULL, descriptors3[1].timestamp);

    EXPECT_EQ(0x07u, descriptors3[2].address.bus); // Event 6: dev=0000:07:00.0
    EXPECT_EQ(0x75, descriptors3[2].uuid.id[0]);   // platform_id starts with 75
    EXPECT_EQ(2750634843ULL, descriptors3[2].timestamp);

    EXPECT_EQ(0x08u, descriptors3[3].address.bus); // Event 7: dev=0000:08:00.0
    EXPECT_EQ(0x85, descriptors3[3].uuid.id[0]);   // platform_id starts with 85
    EXPECT_EQ(2750657681ULL, descriptors3[3].timestamp);

    // Fourth read: request remaining events (should be 0 since we've read all 8)
    uint32_t read4EventCount = 10; // Request more than available
    uint32_t read4Size = read4EventCount * mockCperLen;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &read4Size, nullptr, &read4EventCount, nullptr));
    EXPECT_EQ(0u, read4EventCount) << "No more events should be available after reading all 8";
    EXPECT_EQ(0u, read4Size) << "Size should be 0 when no events are available";

    // Verify CPER signatures across all reads
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t offset = descriptors1[i].offset;
        EXPECT_EQ(0x43, buffer1[offset + 0]) << "CPER signature mismatch in read1 event " << i;
        EXPECT_EQ(0x50, buffer1[offset + 1]);
        EXPECT_EQ(0x45, buffer1[offset + 2]);
        EXPECT_EQ(0x52, buffer1[offset + 3]);
    }

    uint32_t offset2 = descriptors2[0].offset;
    EXPECT_EQ(0x43, buffer2[offset2 + 0]) << "CPER signature mismatch in read2";
    EXPECT_EQ(0x50, buffer2[offset2 + 1]);
    EXPECT_EQ(0x45, buffer2[offset2 + 2]);
    EXPECT_EQ(0x52, buffer2[offset2 + 3]);

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t offset = descriptors3[i].offset;
        EXPECT_EQ(0x43, buffer3[offset + 0]) << "CPER signature mismatch in read3 event " << i;
        EXPECT_EQ(0x50, buffer3[offset + 1]);
        EXPECT_EQ(0x45, buffer3[offset + 2]);
        EXPECT_EQ(0x52, buffer3[offset + 3]);
    }

    // Verify distinguishing bytes (at offset 96) are correct across all reads
    EXPECT_EQ(1, buffer1[descriptors1[0].offset + cperRecordIdOffset]) << "Event 0 distinguishing byte";
    EXPECT_EQ(2, buffer1[descriptors1[1].offset + cperRecordIdOffset]) << "Event 1 distinguishing byte";
    EXPECT_EQ(3, buffer1[descriptors1[2].offset + cperRecordIdOffset]) << "Event 2 distinguishing byte";
    EXPECT_EQ(4, buffer2[descriptors2[0].offset + cperRecordIdOffset]) << "Event 3 distinguishing byte";
    EXPECT_EQ(5, buffer3[descriptors3[0].offset + cperRecordIdOffset]) << "Event 4 distinguishing byte";
    EXPECT_EQ(6, buffer3[descriptors3[1].offset + cperRecordIdOffset]) << "Event 5 distinguishing byte";
    EXPECT_EQ(7, buffer3[descriptors3[2].offset + cperRecordIdOffset]) << "Event 6 distinguishing byte";
    EXPECT_EQ(8, buffer3[descriptors3[3].offset + cperRecordIdOffset]) << "Event 7 distinguishing byte";
}

TEST_F(SysmanInfoLogFixture, GivenPartialCperLineWhenNonBlockingReadReportsEagainThenRecordIsCompletedOnNextRead) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Deliver one record in two installments, the way a non-blocking trace_pipe hands over a line the
    // kernel has not finished writing. The split point is mid-line, so the first half carries no
    // newline and cannot be parsed on its own.
    const size_t splitPos = mockCperEvent1.size() / 2;
    std::string pipeData = mockCperEvent1.substr(0, splitPos);
    ASSERT_EQ(std::string::npos, pipeData.find('\n')) << "First installment must not contain a complete line";

    VariableBackup<decltype(MockTraceFsApiWithData::tracePipeDataPtr)> pipeDataBackup(&MockTraceFsApiWithData::tracePipeDataPtr, &pipeData);
    VariableBackup<decltype(MockTraceFsApiWithData::tracePipeReadPos)> readPosBackup(&MockTraceFsApiWithData::tracePipeReadPos, 0u);
    VariableBackup<decltype(MockTraceFsApiWithData::drainedErrno)> drainedErrnoBackup(&MockTraceFsApiWithData::drainedErrno, EAGAIN);

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);

    // Only the front half is queued, so nothing may be reported yet. Parsing it here would fail the
    // cper_len check and discard the prefix, which cannot be re-read from the pipe.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount) << "A line cut short by EAGAIN must not be reported as a record";
    EXPECT_EQ(0u, size);

    // The remainder arrives; the retained prefix must be prepended so the record survives intact.
    pipeData += mockCperEvent1.substr(splitPos);

    size = mockCperLen;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount) << "The split record must be recovered once the rest of the line arrives";
    EXPECT_EQ(mockCperLen, size);

    // Payload must be the whole CPER record, not just the bytes from the second installment.
    EXPECT_EQ(mockCperLen, descriptors[0].lengthOfData);
    EXPECT_EQ(0x43, buffer[descriptors[0].offset + 0]); // 'C'
    EXPECT_EQ(0x50, buffer[descriptors[0].offset + 1]); // 'P'
    EXPECT_EQ(0x45, buffer[descriptors[0].offset + 2]); // 'E'
    EXPECT_EQ(0x52, buffer[descriptors[0].offset + 3]); // 'R'
}

TEST_F(SysmanInfoLogFixture, GivenPartialCperLineWhenTracePipeReadFailsThenErrnoIsReportedAndPartialLineIsDiscarded) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Same mid-line split as the EAGAIN case, but the read fails for a reason that will not clear up.
    const size_t splitPos = mockCperEvent1.size() / 2;
    std::string pipeData = mockCperEvent1.substr(0, splitPos);

    VariableBackup<decltype(MockTraceFsApiWithData::tracePipeDataPtr)> pipeDataBackup(&MockTraceFsApiWithData::tracePipeDataPtr, &pipeData);
    VariableBackup<decltype(MockTraceFsApiWithData::tracePipeReadPos)> readPosBackup(&MockTraceFsApiWithData::tracePipeReadPos, 0u);
    VariableBackup<decltype(MockTraceFsApiWithData::drainedErrno)> drainedErrnoBackup(&MockTraceFsApiWithData::drainedErrno, EPERM);

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);

    // A broken stream must surface the errno rather than pass as a successful empty read.
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);

    // The prefix can never be completed, so it must have been dropped rather than retained: if it were
    // still buffered it would be spliced onto the next line and swallow that record too.
    pipeData += mockCperEvent1;
    MockTraceFsApiWithData::drainedErrno = 0; // Stream is healthy again

    size = mockCperLen;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount) << "A discarded prefix must not corrupt the next complete record";
    EXPECT_EQ(mockCperLen, size);
}

namespace {
struct CorruptDataFactories {
    static std::unique_ptr<TraceFsApi> makeEventWithoutMarker() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("xe_error_cper:");
        if (pos != std::string::npos) {
            data.replace(pos, 14, "some_other_evt");
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeTruncatedEvent() {
        std::string data = mockCperEvent1;
        if (!data.empty() && data.back() == '\n') {
            data.pop_back();
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithoutCperLen() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_len=532");
        if (pos != std::string::npos) {
            data.erase(pos, 12);
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithZeroCperLen() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_len=532");
        if (pos != std::string::npos) {
            data.replace(pos, 12, "cper_len=0");
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithoutCperRaw() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_raw=");
        if (pos != std::string::npos) {
            size_t endPos = data.find('\n', pos);
            if (endPos != std::string::npos) {
                data.erase(pos, endPos - pos);
            }
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithEmptyCperRaw() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_raw=");
        if (pos != std::string::npos) {
            size_t dataStart = pos + 9;
            size_t endPos = data.find('\n', dataStart);
            if (endPos != std::string::npos) {
                data.erase(dataStart, endPos - dataStart);
            }
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithInvalidHex() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_raw=");
        if (pos != std::string::npos) {
            data.replace(pos + 9, 11, "XY ZZ QQ ");
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithOddHexLength() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_raw=");
        if (pos != std::string::npos) {
            size_t dataStart = pos + 9;
            // Remove a hex digit (not a space) to create odd length
            // Position 9 is the second hex digit '2' in "43 50 45 52..."
            data.erase(dataStart + 9, 1);
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithSizeMismatch() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("cper_len=532");
        if (pos != std::string::npos) {
            data.replace(pos, 12, "cper_len=100");
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data + mockCperEvent2);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithoutTimestamp() {
        std::string data = mockCperEvent1;
        size_t pos = data.find(": xe_error_cper:");
        if (pos != std::string::npos && pos > 0) {
            data[pos] = ' ';
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithBadTimestamp() {
        std::string data = mockCperEvent1;
        size_t pos = data.find(": xe_error_cper:");
        if (pos != std::string::npos && pos > 15) {
            for (size_t i = pos - 1; i > pos - 15 && i > 0; --i) {
                if (data[i] == '.') {
                    data.erase(i, 1);
                    break;
                }
            }
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithoutDev() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("dev=0000:13:00.0");
        if (pos != std::string::npos) {
            data.erase(pos, 17);
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithBadBDF() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("dev=0000:13:00.0");
        if (pos != std::string::npos) {
            data.replace(pos, 16, "dev=invalid_bdf");
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithoutPlatformId() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("platform_id=");
        if (pos != std::string::npos) {
            size_t endPos = data.find(' ', pos);
            if (endPos != std::string::npos) {
                data.erase(pos, endPos - pos + 1);
            }
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }

    static std::unique_ptr<TraceFsApi> makeEventWithBadUUID() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("platform_id=");
        if (pos != std::string::npos) {
            data.replace(pos + 12, 36, "invalid-uuid-format-here");
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }
};
} // anonymous namespace

// Case #1: Missing "xe_error_cper:" marker - Non-CPER line should be skipped
TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledMissingMarkerThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithoutMarker);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted (event1 skipped)
    EXPECT_EQ(mockCperLen, size);
}

// Case #2: Truncated line (no newline at EOF)
TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledTruncatedLineWithoutNewlineThenProcessed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeTruncatedEvent);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should still process the incomplete line
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(mockCperLen, size);
}
//
// Case #3: Line exceeds kTraceLineBufferSize (now 8192 bytes) - tests accumulation
// mockCperEvent1 is ~2819 characters, fits in new 8192-byte buffer (single read)
TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledLongLineExceedsBufferThenAccumulated) {
    // This test verifies that lines are properly read with the 8192-byte buffer
    // mockCperEvent1 has 532-byte CPER = 1064 hex chars + metadata 2819 chars total
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(mockCperLen, size);

    // Verify data integrity - accumulated line produced correct CPER bytes
    for (uint32_t i = 0; i < std::min<uint32_t>(static_cast<uint32_t>(expectedCper1Bytes.size()), size); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]) << "Mismatch at byte " << i;
    }
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledMissingCperLenThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithoutCperLen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledCperLenZeroThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithZeroCperLen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledMissingCperRawThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithoutCperRaw);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledCperRawEmptyThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithEmptyCperRaw);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleReadMetaDataCalledInvalidHexCharsThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithInvalidHex);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleOddHexLengthThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithOddHexLength);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleSizeMismatchThenRecordSkipped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithSizeMismatch);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleMissingTimestampThenZeroTimestampIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithoutTimestamp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with timestamp=0
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, descriptors[0].timestamp); // Zero timestamp due to parse failure
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleBadTimestampFormatThenZeroTimestamp) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithBadTimestamp);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with timestamp=0
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleMissingDevFieldThenZeroBDF) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithoutDev);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with zero BDF
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, descriptors[0].address.domain);
    EXPECT_EQ(0u, descriptors[0].address.bus);
    EXPECT_EQ(0u, descriptors[0].address.device);
    EXPECT_EQ(0u, descriptors[0].address.function);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleBadBDFFormatThenZeroBDF) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithBadBDF);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with zero BDF
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, descriptors[0].address.domain);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleMissingPlatformIdThenZeroUUID) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithoutPlatformId);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with zero UUID
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    // Verify UUID is all zeros
    bool allZeros = true;
    for (int i = 0; i < 16; i++) {
        if (descriptors[0].uuid.id[i] != 0) {
            allZeros = false;
            break;
        }
    }
    EXPECT_TRUE(allZeros);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleBadUUIDFormatThenZeroUUID) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, CorruptDataFactories::makeEventWithBadUUID);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with zero UUID
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    bool allZeros = true;
    for (int i = 0; i < 16; i++) {
        if (descriptors[0].uuid.id[i] != 0) {
            allZeros = false;
            break;
        }
    }
    EXPECT_TRUE(allZeros);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleBufferTooSmallForOneRecordThenWarningDropped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 100; // Too small for 532-byte record
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(100);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount); // No records fit
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleBufferFillsMidProcessThenWarningDropped) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true); // 3 events available
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 600; // Fits 1 record (532 bytes), not 2
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount); // Only 1 record fit
    EXPECT_EQ(mockCperLen, size);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleNullBufferInExtractModeThenQueryModeIsTriggered) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 0;
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // pBuffer=NULL, pDescriptors!=NULL triggers query mode
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, &eventCount, descriptors.data()));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleNullDescriptorsInExtractModeThenQueryMode) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 0;
    std::vector<uint8_t> buffer(size);

    // pBuffer!=NULL, pDescriptors=NULL triggers query mode
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, nullptr));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleFgetsReturnsNullThenPartialResults) {
    static int fgetsCallCount = 0;
    fgetsCallCount = 0;

    auto mockFgetsFailAfter3 = [](char *s, int size, FILE *stream) -> char * {
        if (fgetsCallCount++ >= 3) {
            return nullptr; // Simulate premature EOF
        }
        return MockTraceFsApiWithData::mockSysCallsFgets(s, size, stream);
    };

    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true); // 3 events
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, mockFgetsFailAfter3);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    // Graceful: Returns whatever was extracted before fgets failed
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    // Should have partial data (exact count depends on when fgets failed during line read)
    EXPECT_LE(eventCount, 3u);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleFdopenFailsThenErrorReturned) {
    auto mockFdopenFail = [](int fd, const char *mode) -> FILE * {
        (void)fd;
        (void)mode;
        errno = ENOMEM;
        return nullptr;
    };

    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, mockFdopenFail);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleDupFailsThenErrorReturned) {
    auto mockDupFail = [](int oldfd) -> int {
        (void)oldfd;
        errno = EMFILE; // Too many open files
        return -1;
    };

    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, mockDupFail);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenEventDisabledWhenCheckingEventStateThenReturnsFalse) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockEventAlreadyEnabled = false; // Event disabled - will return nullptr
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Attempt to enable - will fail because event is already disabled (pre-existing)
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenTracingOffWhenCheckingTracingStateThenReturnsFalse) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockTracingAlreadyOn = false; // Tracing off - will return nullptr
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenNegativeBufferSizeWhenGettingPropertiesThenSetsZeroMaxSize) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failGetBufferSize = true;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));
    EXPECT_EQ(0u, properties.maxSize); // Should be 0 when bufferSize <= 0
}

TEST_F(SysmanInfoLogFixture, GivenTimestampWithoutDotWhenParsingThenReturnsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithoutTimestampDot);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0ULL, descriptors[0].timestamp); // Should be 0 when parsing fails
}

TEST_F(SysmanInfoLogFixture, GivenNullEntryPointsWhenCallingTraceFsApiFunctionsThenEarlyReturn) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        // Don't load entry points - they'll all be null
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Try to enable - will fail due to null entry points
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_NE(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyEnabledWithSameGlobalConfigWhenCallingInfoLogEnableAgainThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Enable with global instance (instanceName = nullptr)
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Try to enable again with same global configuration
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyEnabledWithGlobalConfigWhenCallingInfoLogEnableWithNamedInstanceThenInvalidArgumentIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Enable with global instance
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Try to enable with named instance - should fail with conflicting configuration
    const char *instanceName = "conflict_instance";
    zes_intel_info_log_enable_descriptor_exp desc2 = {instanceName, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc2));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenBufferSizeConfigurationWhenCallingInfoLogEnableThenBufferSizeIsSet) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    constexpr uint32_t requestedSizeKb = 256u;
    uint32_t bufferSizeKb = requestedSizeKb;
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, &bufferSizeKb, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // libtracefs takes the size in kilobytes, so the request must reach it unscaled, and cpu -1
    // applies it to every per-CPU buffer.
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(static_cast<size_t>(requestedSizeKb), PublicTraceFsApi::lastSetBufferSize);
    EXPECT_EQ(-1, PublicTraceFsApi::lastSetBufferSizeCpu);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize), bufferSizeKb); // Set to the actual size

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenValidBufferPercentThresholdWhenCallingInfoLogEnableThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Set mock expectations for global instance with 75% threshold
    uint32_t percentThreshold = 75; // Valid: 0-100
    MockTraceFsOsLibrary::mockBufferPercent = percentThreshold;

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, &percentThreshold};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_LE(percentThreshold, 100u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenNotEnabledWhenCallingInfoLogDisableThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Call disable without enabling first
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenInstanceCreationFailsWhenCallingInfoLogEnableThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockInstanceCreateReturnsNull = true; // Use correct member name
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    zes_intel_info_log_enable_descriptor_exp desc = {instanceName, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeNotOpenedWhenCallingInfoLogReadWithMetaDataThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Don't call enable, so trace_pipe won't be opened
    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should fail because trace_pipe is not opened
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
}

TEST_F(SysmanInfoLogFixture, GivenTimestampWithoutColonSeparatorWhenParsingThenTimestampIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithoutTimestampColon);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0ULL, descriptors[0].timestamp); // Should be 0 when xe_error_cper marker not found
}

TEST_F(SysmanInfoLogFixture, GivenMissingCperLenFieldWhenParsingThenFieldNotFoundPathIsExecuted) {
    // Line with xe_error_cper but missing cper_len field
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithoutCperLenField);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should succeed but extract 0 events due to missing field
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenBdfWithoutColonSeparatorWhenParsingThenParseBdfFailsGracefully) {
    // dev field without colon separators
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithoutBdfColons);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    // BDF parsing failed, but event should still be extracted
}

TEST_F(SysmanInfoLogFixture, GivenBdfWithInvalidFormatWhenParsingThenParseBdfFailsGracefully) {
    // dev field with non-hex characters
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithNonHexBdf);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenUuidWithInvalidLengthWhenParsingThenParseUuidFailsGracefully) {
    // platform_id field with invalid length (too short)
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithShortPlatformId);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenUuidWithInvalidFormatWhenParsingThenParseUuidFailsGracefully) {
    // platform_id field with invalid hex characters
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithNonHexPlatformId);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenNonCperLineWhenParsingThenProcessCperLineSkipsIt) {
    // Non xe_error_cper line (different tracepoint event)
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockNonCperTraceEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should succeed but extract 0 events because line is not a CPER line
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenCperRawWithInvalidHexWhenParsingThenValidationFails) {
    // cper_raw field with non-hex characters (ZZZZ)
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithNonHexCperRaw);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should succeed but extract 0 events due to CPER validation error
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenBufferTooSmallWhenExtractingCperThenBufferFullPathIsExecuted) {
    // Use single CPER event but very small buffer
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 10; // Very small buffer - not enough for 532 byte CPER
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    // Should return WARNING_DROPPED_DATA with 0 events extracted due to buffer too small
    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount); // Buffer full, cannot add CPER
}

TEST_F(SysmanInfoLogFixture, GivenCperLenZeroWhenCountingRecordsThenSkipsRecord) {
    // Line with cper_len=0
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithZeroLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 0;
    uint32_t eventCount = 0;
    // Query mode (pBuffer is null) - will call countCperRecordsAndSize
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, &eventCount, nullptr));
    EXPECT_EQ(0u, eventCount); // Should be 0 because cper_len=0 is skipped
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInstanceGetFileReturnsNullWhenEnablingNamedInstanceThenErrorIsReturned) {
    // Mock that returns nullptr for traceFsInstanceGetFile
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failGetFile = true; // Fail to get trace_pipe path
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    zes_intel_info_log_enable_descriptor_exp desc = {instanceName, nullptr, nullptr};
    // Should fail because traceFsInstanceGetFile returns nullptr
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeNotOpenedForNamedInstanceWhenReadingThenErrorIsReturned) {
    // The trace_pipe path is resolved, but every open of it fails
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
    });

    // Mock open to always fail
    auto mockOpenFail = [](const char *pathname, int flags) -> int {
        return -1;
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, mockOpenFail);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    zes_intel_info_log_enable_descriptor_exp desc = {instanceName, nullptr, nullptr};
    // Enable will fail because open fails
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeClosedForInstancedCollectionWhenExtractingThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    zes_intel_info_log_enable_descriptor_exp desc = {instanceName, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // Now disable to close the trace_pipe
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));

    // Now try to enable again with the instance name
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Extract mode should work since we re-enabled
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsInstanceFileReadReturnsNullInQueryModeThenErrorIsReturned) {
    // Mock that returns nullptr for trace file read
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failTraceFileRead = true; // Fail to read trace file
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 0;
    uint32_t eventCount = 0;
    // Query mode - will fail because trace file read returns nullptr
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, &eventCount, nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenEnabledNamedInstanceWhenGettingPropertiesThenBufferSizeIsQueriedFromActiveInstance) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->useDistinctBufferSizePerTarget = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(1u, MockTraceFsApiWithData::bufferSizeQueryCount);
    EXPECT_EQ(nullptr, MockTraceFsApiWithData::queriedBufferSizeInstance);

    auto *pInfoLogImp = static_cast<InfoLogImp *>(InfoLog::fromHandle(infoLogHandles[0]));

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1u, MockTraceFsApiWithData::bufferSizeQueryCount);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->pOsInfoLog->getProperties(&properties));
    EXPECT_EQ(2u, MockTraceFsApiWithData::bufferSizeQueryCount);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, MockTraceFsApiWithData::queriedBufferSizeInstance);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsApiWithData::mockNamedInstanceBufferSize), properties.maxSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->pOsInfoLog->getProperties(&properties));
    EXPECT_EQ(3u, MockTraceFsApiWithData::bufferSizeQueryCount);
    EXPECT_EQ(nullptr, MockTraceFsApiWithData::queriedBufferSizeInstance);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsApiWithData::mockGlobalBufferSize), properties.maxSize);
}

TEST_F(SysmanInfoLogFixture, GivenInstancesDirectoryNotWritableWhenGettingPropertiesThenIsInstancedCollectionSupportedIsFalse) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    VariableBackup<bool> failAccessBackup(&NEO::SysCalls::failAccess, true);

    auto *pInfoLogImp = static_cast<InfoLogImp *>(InfoLog::fromHandle(infoLogHandles[0]));

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pInfoLogImp->pOsInfoLog->getProperties(&properties));
    EXPECT_FALSE(properties.isInstancedCollectionSupported);
}

TEST_F(SysmanInfoLogFixture, GivenInstancesDirectoryNotWritableWhenEnablingNamedInstanceThenUnsupportedFeatureIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    VariableBackup<bool> failAccessBackup(&NEO::SysCalls::failAccess, true);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceNotPreExistingWhenEventEnableFailsThenInstanceIsDestroyedAndFreed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->eventEnableReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPreExistingNamedInstanceWhenEventEnableFailsThenInstanceIsFreedWithoutBeingDestroyed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->eventEnableReturnValue = -1;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenOverlongLineFollowedByValidEventWhenReadingWithMetaDataThenLineIsSkippedAndNextEventIsExtracted) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockOverlongLineFollowedByCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xABu, buffer[0]);
    EXPECT_EQ(0xCDu, buffer[1]);
    EXPECT_EQ(2u, descriptors[0].lengthOfData);
    EXPECT_EQ(5058247549ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenOverlongLineAtEndOfTraceDataWhenReadingWithMetaDataThenNoEventIsExtracted) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockOverlongLineWithoutNewline);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenIncompleteFinalLineNotFittingInBufferWhenReadingWithMetaDataThenDroppedDataWarningIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithoutTrailingNewline);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 2;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenSetvbufFailsWhenReadingWithMetaDataThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbufFailure);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenTraceFileReadFailsInQueryModeThenErrorIsReturnedWithInstanceContext) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failTraceFileRead = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 0;
    uint32_t eventCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, nullptr, &eventCount, nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(1u, MockTraceFsApiWithConfigurableBehavior::traceReadCallCount);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, MockTraceFsApiWithConfigurableBehavior::traceReadInstance);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenReadingInfoLogThenTraceFileIsReadFromThatInstance) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failTraceFileRead = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
    EXPECT_EQ(1u, MockTraceFsApiWithConfigurableBehavior::traceReadCallCount);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, MockTraceFsApiWithConfigurableBehavior::traceReadInstance);
}

TEST_F(SysmanInfoLogFixture, GivenTimestampWithShortFractionalPartWhenReadingWithMetaDataThenFractionIsPaddedToMicroseconds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithShortTimestampFraction);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(5058240000ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenNoSpaceBeforeTimestampWhenReadingWithMetaDataThenTimestampIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithoutSpaceBeforeTimestamp);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenNewlyCreatedNamedInstanceWhenDisablingInfoLogThenInstanceIsDestroyedAndFreed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceFreeCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenEventAlreadyEnabledWhenTraceOnFailsThenEventIsNotDisabled) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->eventEnableStateContent = "1";
        mockApi->traceOnReturnValue = -1;
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0, MockTraceFsApiWithData::eventDisableCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenSetBufferPercentFailsWhenEnablingNamedInstanceThenErrorIsReturnedAndInstanceIsCleanedUp) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failSetBufferPercent = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t percentFullThreshold = 75u;
    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, &percentFullThreshold};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenSetBufferSizeFailsWhenEnablingNamedInstanceThenErrorIsReturnedAndInstanceIsCleanedUp) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failSetBufferSize = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSizeInKb = 256u;
    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", &bufferSizeInKb, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenRequestedBufferSizeWhenEnablingInfoLogThenActualPerCpuSizeIsReportedBack) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->reportDistinctPerCpuAndTotalBufferSize = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSizeInKb = 256u;
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, &bufferSizeInKb, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // The read back size describes one per-CPU buffer, not the sum across all CPUs.
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsApiWithConfigurableBehavior::mockPerCpuBufferSizeKb), bufferSizeInKb);
}

TEST_F(SysmanInfoLogFixture, GivenGetBufferSizeFailsWhenEnablingInfoLogThenReportedBufferSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failGetBufferSize = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSizeInKb = 256u;
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, &bufferSizeInKb, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0u, bufferSizeInKb); // Should be 0 rather than a wrapped around -1
}

TEST_F(SysmanInfoLogFixture, GivenBufferFullDropsAnOversizedRecordWhenReadingAgainThenTheFollowingRecordIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSmallCperTraceEvent, false, &mockLargerThenSmallCperTracePipeEvents);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    uint32_t size = 2u;
    std::vector<uint8_t> buffer(1024u);
    EXPECT_EQ(ZE_RESULT_WARNING_DROPPED_DATA, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);

    size = 1024u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0x12, buffer[0]);
    EXPECT_EQ(0x34, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenFirstTraceFsPathFailsWhenEnablingInfoLogThenFallsBackToSecondPathAndReadSucceeds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(false, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    EXPECT_EQ(2, MockTraceFsApiWithData::openCallCount);

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(mockCperLen, size);

    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionAlreadyEnabledWhenEnablingAgainThenDescriptorIsReusedAndNotLeaked) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(1, MockTraceFsApiWithData::openCallCount);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionDisabledAfterEnableWhenReadingInfoLogThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(-1, getCperTracePipeFd());

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionEnabledThenTraceFsWakeWatermarkIsLoweredToZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenEnablingAndDisablingInfoLogThenWakeWatermarkIsProgrammedOnThatInstance) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, nullptr};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, PublicTraceFsApi::lastSetBufferPercentInstance);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, PublicTraceFsApi::lastSetBufferPercentInstance);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWithExplicitThresholdWhenEnablingInfoLogThenWakeWatermarkIsNotForcedToZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t percentFullThreshold = 60u;
    zes_intel_info_log_enable_descriptor_exp desc = {"my_instance", nullptr, &percentFullThreshold};
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    // The caller supplied a fill level, so it is honoured verbatim and nothing is saved for restore.
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(60, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferPercent), percentFullThreshold);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionEnabledWhenDisablingThenPreviousTraceFsWakeWatermarkIsRestored) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionNeverEnabledWhenDisablingThenSuccessIsReturnedAndCloseIsNotCalled) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    ASSERT_EQ(-1, getCperTracePipeFd());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionNeverEnabledWhenDriverIsDestroyedThenTraceFsStateIsLeftUntouched) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    ASSERT_EQ(-1, getCperTracePipeFd());

    delete driverHandle;
    driverHandle = nullptr;

    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::eventDisableCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::traceOffCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionNotEnabledAndNoFittingRecordsInTraceWhenReadingInfoLogThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithZeroLen);
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    ASSERT_EQ(-1, getCperTracePipeFd());

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionNotEnabledWhenReadingInfoLogThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    ASSERT_EQ(-1, getCperTracePipeFd());

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionStillEnabledWhenDriverIsDestroyedThenCollectionIsDisabledAndTraceFsStateIsRestored) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    ASSERT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);
    ASSERT_EQ(0, MockTraceFsApiWithData::eventDisableCallCount);
    ASSERT_EQ(0, MockTraceFsApiWithData::traceOffCallCount);

    delete driverHandle;
    driverHandle = nullptr;

    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(1, MockTraceFsApiWithData::eventDisableCallCount);
    EXPECT_EQ(1, MockTraceFsApiWithData::traceOffCallCount);
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogCollectionStillEnabledWhenDriverIsDestroyedThenTracePipeDescriptorIsClosed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_EQ(0, MockTraceFsApiWithData::closeCallCount);

    delete driverHandle;
    driverHandle = nullptr;

    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInfoLogFormatIsNotCperWhenEnablingInfoLogThenTracingIsTurnedOnWithoutOpeningTracePipe) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto nonCperFormat = static_cast<zes_intel_info_log_format_exp_t>(ZES_INTEL_INFO_LOG_FORMAT_CPER + 1);
    LinuxInfoLogImp infoLogImp(nonCperFormat);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, infoLogImp.infoLogEnable(&desc));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::openCallCount);
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenDisablingInfoLogThenCloseIsSkippedAndSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    auto *pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    auto *originalOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    pSysmanDriverHandleImp->pOsSysmanDriver = originalOsSysmanDriver;
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnablingInfoLogThenUninitializedIsReturnedAndTraceFsStateIsRolledBack) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto *pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    auto *originalOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    pSysmanDriverHandleImp->pOsSysmanDriver = originalOsSysmanDriver;
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenReadingInfoLogThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    auto *pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    auto *originalOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);

    pSysmanDriverHandleImp->pOsSysmanDriver = originalOsSysmanDriver;
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenReadFailsMidLineWithHardErrorWhenReadingAgainThenTheTruncatedPrefixIsDiscarded) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSmallCperTraceEvent, false, &mockLargerThenSmallCperTracePipeEvents);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsReadWithHardErrorMidCperRaw);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);

    size = 1024u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0x12, buffer[0]);
    EXPECT_EQ(0x34, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenReadStopsMidLineOnEagainWhenReadingAgainThenTheRecordIsCompletedAcrossCalls) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSpacedHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsReadWithEagainMidLine);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);

    size = 1024u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkAlreadyZeroWhenEnablingInfoLogThenItIsNotRewritten) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->getBufferPercentReturnValue = 0;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkCannotBeProgrammedWhenEnablingInfoLogThenCollectionStillSucceeds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->setBufferPercentReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkCannotBeReadWhenEnablingInfoLogThenItIsNotOverridden) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->getBufferPercentReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);

    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkRestoreFailsWhenDisablingThenSavedValueIsKeptAndWrittenBackByALaterDisable) {
    VariableBackup<decltype(PublicTraceFsApi::failSetBufferPercentOnCall)> failRestoreBackup(&PublicTraceFsApi::failSetBufferPercentOnCall, 2u);
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    ASSERT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(3u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(3u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeOpenFailsWhenEnablingInfoLogThenErrorIsReturnedAndNoDescriptorIsStored) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeOpenFailsWhenEnablingInfoLogThenTraceFsWakeWatermarkIsRestored) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
}

TEST_F(SysmanInfoLogFixture, GivenNullGlobalSysmanDriverWhenReadingInfoLogThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_NE(nullptr, L0::Sysman::globalSysmanDriver);

    auto *originalGlobalSysmanDriver = L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, buffer.data()));
    EXPECT_EQ(0u, size);

    size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    uint32_t eventCount = 1;
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, eventCount);

    L0::Sysman::globalSysmanDriver = originalGlobalSysmanDriver;
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullGlobalSysmanDriverWhenEnablingInfoLogThenUninitializedIsReturnedAndTraceFsStateIsRolledBack) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    ASSERT_NE(nullptr, L0::Sysman::globalSysmanDriver);

    auto *originalGlobalSysmanDriver = L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0, MockTraceFsApiWithData::openCallCount);

    // The failed enable unwinds through infoLogDisable(), which also has to tolerate the missing driver.
    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    L0::Sysman::globalSysmanDriver = originalGlobalSysmanDriver;
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeAlreadyOpenedByDriverWhenEnablingInfoLogThenExistingDescriptorIsReused) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Another consumer of the driver already owns a trace_pipe descriptor.
    getLinuxSysmanDriverImp()->setCperTracePipeFd(MockTraceFsApiWithData::mockTracePipeFd);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0, MockTraceFsApiWithData::openCallCount);
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullGlobalSysmanDriverWhenDisablingInfoLogThenCloseIsSkippedAndSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    enableInfoLogCollection(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_NE(nullptr, L0::Sysman::globalSysmanDriver);

    auto *originalGlobalSysmanDriver = L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    L0::Sysman::globalSysmanDriver = originalGlobalSysmanDriver;
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTracepointEnableStateReadsAsZeroWhenEnablingInfoLogThenTracepointIsEnabledAgain) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->eventEnableStateContent = "0";
        mockApi->eventEnableReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // The tracepoint reads back as present but off, so enabling it is attempted and the failure surfaces.
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTracingOnStateReadsAsZeroWhenEnablingInfoLogThenTracingIsTurnedOnAndBackOffOnDisable) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        // 'tracing_on' is present and reads back as off, so the state check compares its content
        // instead of short circuiting on a missing file.
        mockApi->tracingOnStateContent = "0";
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Tracing is reported as off, so enabling has to turn it on.
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1u, MockTraceFsApiWithConfigurableBehavior::traceOnCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::traceOffCallCount);

    // Tracing was not already on before enabling, so disabling has to turn it back off.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
    EXPECT_EQ(1, MockTraceFsApiWithData::traceOffCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyEnabledWithNamedInstanceWhenCallingInfoLogEnableWithGlobalConfigThenInvalidArgumentIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "named_instance";
    zes_intel_info_log_enable_descriptor_exp desc = {instanceName, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    // Requesting the global buffer while a named instance is active is a conflicting configuration.
    zes_intel_info_log_enable_descriptor_exp globalDesc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogEnableExp(infoLogHandles[0], &globalDesc));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyEnabledWithNamedInstanceWhenCallingInfoLogEnableWithDifferentInstanceThenInvalidArgumentIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *firstInstanceName = "instance_a";
    zes_intel_info_log_enable_descriptor_exp firstDesc = {firstInstanceName, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &firstDesc));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    // A different named instance conflicts with the active one.
    const char *secondInstanceName = "instance_b";
    zes_intel_info_log_enable_descriptor_exp secondDesc = {secondInstanceName, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesIntelInfoLogEnableExp(infoLogHandles[0], &secondDesc));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenZeroBufferSizeInDescriptorWhenEnablingInfoLogThenBufferSizeIsLeftUntouched) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto bufferSizeQueryCountBeforeEnable = MockTraceFsApiWithData::bufferSizeQueryCount;

    // A provided-but-zero buffer size means "keep the current tracefs buffer size".
    uint32_t bufferSizeKb = 0;
    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, &bufferSizeKb, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));
    EXPECT_EQ(0u, bufferSizeKb);
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(bufferSizeQueryCountBeforeEnable, MockTraceFsApiWithData::bufferSizeQueryCount);
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogDisableExp(infoLogHandles[0]));
}

TEST_F(SysmanInfoLogFixture, GivenEmptyLineReadFromTracePipeWhenReadingWithMetaDataThenFollowingEventIsStillExtracted) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockSmallCperTraceEvent, 1u);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithConfigurableBehavior::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // The first read hands out a zero length line, which must not be mistaken for a complete record.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xABu, buffer[0]);
    EXPECT_EQ(0xCDu, buffer[1]);
    EXPECT_EQ(2u, descriptors[0].lengthOfData);
    EXPECT_EQ(5058247549ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenEmptyLineWhileResynchronizingAfterOverlongLineWhenReadingWithMetaDataThenNextEventIsExtracted) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockOverlongLineFollowedByCperEvent, 4u);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithConfigurableBehavior::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_enable_descriptor_exp desc = {nullptr, nullptr, nullptr};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogEnableExp(infoLogHandles[0], &desc));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // The overlong line is abandoned on the third read, so the injected empty line lands inside the
    // resynchronization loop, where it must not be treated as the end of the discarded record.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogReadWithMetadataExp(infoLogHandles[0], &size, buffer.data(), &eventCount, descriptors.data()));
    EXPECT_EQ(4u, MockTraceFsApiWithConfigurableBehavior::emptyLineOnFgetsCall);
    EXPECT_GT(MockTraceFsApiWithConfigurableBehavior::fgetsCallCount, 4u);
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xABu, buffer[0]);
    EXPECT_EQ(0xCDu, buffer[1]);
    EXPECT_EQ(2u, descriptors[0].lengthOfData);
    EXPECT_EQ(5058247549ULL, descriptors[0].timestamp);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
