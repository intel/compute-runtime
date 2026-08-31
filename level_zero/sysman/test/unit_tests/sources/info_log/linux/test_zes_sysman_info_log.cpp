/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_instance_imp.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/info_log/linux/mock_sysman_info_log.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include <cerrno>

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t handleCount = 1u;
static constexpr uint32_t maxRecordsPerRead = 64u;
static constexpr uint64_t noTimeout = UINT64_MAX;
// An hour, in the milliseconds getCurrentTimeInMs() reports. Long enough that the collection deadline
// is never reached within a test, while staying well clear of a uint64 overflow when added to 'now'.
static constexpr uint64_t largeTimeoutMs = 60ULL * 60ULL * 1000ULL;

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
            auto *pLinuxSysmanDriverImp = getLinuxSysmanDriverImp();
            if (pLinuxSysmanDriverImp != nullptr) {
                for (auto fd : pLinuxSysmanDriverImp->getCperTracePipeFds()) {
                    pLinuxSysmanDriverImp->unregisterCperTracePipeFd(fd);
                }
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

    static zes_intel_info_log_instance_exp_desc_t makeInstanceDesc(uint32_t *pBufferSize = nullptr) {
        zes_intel_info_log_instance_exp_desc_t desc = {};
        desc.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_INSTANCE_EXP_DESC;
        desc.pBufferSize = pBufferSize;
        return desc;
    }

    zes_intel_info_log_instance_handle_t createInfoLogInstance(zes_intel_info_log_handle_t hInfoLog, const char *pInstanceName = nullptr) {
        auto desc = makeInstanceDesc();
        zes_intel_info_log_instance_handle_t hInstance = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(hInfoLog, pInstanceName, &desc, &hInstance));
        EXPECT_NE(nullptr, hInstance);
        return hInstance;
    }

    ze_result_t readInfoLogData(zes_intel_info_log_instance_handle_t hInstance, uint32_t *pSize, uint8_t *pBuffer) {
        uint32_t recordCount = maxRecordsPerRead;
        std::vector<zes_intel_info_log_metadata_exp> descriptors(recordCount);
        return zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, pSize, pBuffer, &recordCount, descriptors.data(), nullptr);
    }

    ze_result_t peekInfoLogData(zes_intel_info_log_instance_handle_t hInstance, uint32_t *pSize, uint8_t *pBuffer) {
        uint32_t recordCount = maxRecordsPerRead;
        std::vector<zes_intel_info_log_metadata_exp> descriptors(recordCount);
        return zesIntelInfoLogInstancePeekWithMetadataExp(hInstance, noTimeout, pSize, pBuffer, &recordCount, descriptors.data(), nullptr);
    }

    L0::Sysman::LinuxSysmanDriverImp *getLinuxSysmanDriverImp() {
        return static_cast<L0::Sysman::LinuxSysmanDriverImp *>(
            static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle)->pOsSysmanDriver);
    }

    int getCperTracePipeFd() {
        auto *pLinuxSysmanDriverImp = getLinuxSysmanDriverImp();
        return (pLinuxSysmanDriverImp != nullptr) ? pLinuxSysmanDriverImp->getCperTracePipeFd() : -1;
    }

    size_t getCperTracePipeFdCount() {
        auto *pLinuxSysmanDriverImp = getLinuxSysmanDriverImp();
        return (pLinuxSysmanDriverImp != nullptr) ? pLinuxSysmanDriverImp->getCperTracePipeFds().size() : 0u;
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
    EXPECT_TRUE(properties.isPeekSupported);
    EXPECT_TRUE(properties.isNamedInstancedCollectionSupported);
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnumeratingInfoLogsThenErrorIsReturned) {
    auto *pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    auto *originalOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pSysmanDriverHandleImp->enumInfoLogs(&count, nullptr));

    pSysmanDriverHandleImp->pOsSysmanDriver = originalOsSysmanDriver;
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCreatingInstanceSuccessfullyThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_NE(nullptr, hInstance);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogInstanceWhenDeletingItThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto hInstance = createInfoLogInstance(infoLogHandles[0]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceDeleteExp(hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenBufferSizeIsZeroThenTotalsAreQueried) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    uint32_t zeroSize = 0;
    std::vector<uint8_t> buffer(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &zeroSize, buffer.data()));
    EXPECT_EQ(mockCperLen, zeroSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenReadingMultipleCperDataThenSuccessAndAllDataIsReturned) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    // Total size is 3 * 532 = 1596 bytes
    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    // Allocate buffer that can fit only 1 CPER (532 bytes) but not 2 (1064 bytes)
    uint32_t size = 600u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));

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
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInvalidHexCharacterInCperRawWhenReadingInfoLogThenZeroBytesAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockInvalidHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenMissingCperLenFieldWhenReadingInfoLogThenZeroBytesAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockMissingFieldCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCompactHexCperRawWithNoSpacesWhenReadingInfoLogThenDataIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCompactHexCperEvent);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
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
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
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
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenCperEventWithZeroLengthWhenReadingInfoLogThenRecordIsSkippedAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithZeroLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperRawWithEmptyValueWhenReadingInfoLogThenWhileConditionIsFalseAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithEmptyCperRaw);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperLenFieldEmptyAtEndOfLineWhenReadingInfoLogThenWhileConditionIsFalseAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithEmptyCperLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenCperRawByteCountMismatchesLenFieldWhenReadingInfoLogThenCperDataSizeNotEqualCperLenAndSizeIsZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockCperEventWithMismatchedLen);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFileReadReturnsNullWhenPeekingInfoLogThenTraceDataIsNullAndErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(MockTraceFsApiWithBadCperData::emptyStr, false, nullptr, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, peekInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeDeliversExtraLargerRecordAfterFittingRecordsWhenReadingInfoLogThenTheFittingRecordsAreReturnedFirst) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockTwoSmallCperTraceEvents, false, &mockTwoSmallPlusOneLargerCperTracePipeEvents);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    // Buffer fits 3x 2-byte records (6 bytes) but not 2x 2-byte + 1x 4-byte (8 bytes)
    uint32_t size = 7u;
    std::vector<uint8_t> buffer(16u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    // First 2 records (4 bytes) were written before the 3rd record overflowed
    EXPECT_EQ(4u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
    EXPECT_EQ(0xEF, buffer[2]);
    EXPECT_EQ(0x01, buffer[3]);

    size = 16u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(4u, size);
    EXPECT_EQ(0x11, buffer[0]);
    EXPECT_EQ(0x22, buffer[1]);
    EXPECT_EQ(0x33, buffer[2]);
    EXPECT_EQ(0x44, buffer[3]);
}

TEST_F(SysmanInfoLogFixture, GivenGlobalInstanceWhenAccessChecksFailThenSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Global enable takes no access based decision, so it still succeeds when access() fails.
    VariableBackup<bool> failAccessBackup(&NEO::SysCalls::failAccess, true);
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenGlobalWithTracingAlreadyOnWhenCreatingInstanceThenSuccessIsReturnedAndTraceOnIsSkipped) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenGlobalWithEventAlreadyEnabledWhenCreatingInstanceThenSuccessIsReturnedAndEventEnableIsSkipped) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenEventAlreadyEnabledWhenDeletingInstanceThenEventDisableIsSkippedAndSuccessIsReturned) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenTracingAlreadyOnWhenDeletingInstanceThenTraceOffIsSkippedAndSuccessIsReturned) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenNullBufferWhenQueryingWithMetaDataThenSizeAndCountAreReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    uint32_t size = 0;
    uint32_t eventCount = 0;
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, nullptr, &readStatus));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.hasDataToRead);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenNullBufferAndMultipleEventsWhenQueryingWithMetaDataThenCorrectCountIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    uint32_t size = 0;
    uint32_t eventCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, nullptr, nullptr));
    EXPECT_EQ(3u * mockCperLen, size);
    EXPECT_EQ(3u, eventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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

    // UUID: fru_id=fbaf4690-4190-2451-8614-92550d9e9da6
    static const uint8_t expectedUuid[16] = {0xfb, 0xaf, 0x46, 0x90, 0x41, 0x90, 0x24, 0x51,
                                             0x86, 0x14, 0x92, 0x55, 0x0d, 0x9e, 0x9d, 0xa6};
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(expectedUuid[i], descriptors[0].uuid.id[i]) << "UUID mismatch at byte " << i;
    }

    // Timestamp: 5058.247549 -> 5058*1_000_000_000 + 247549*1000 = 5058247549000 ns
    EXPECT_EQ(5058247549000ULL, descriptors[0].timestamp);
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    // Query mode: get total count and size
    uint32_t querySize = 0;
    uint32_t queryEventCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &querySize, nullptr, &queryEventCount, nullptr, nullptr));
    EXPECT_EQ(8u, queryEventCount) << "Should have 8 xe_error_cper events total";
    EXPECT_EQ(8u * mockCperLen, querySize) << "Total size should be 8 * 532";

    // First read: request 3 events
    uint32_t read1EventCount = 3;
    uint32_t read1Size = read1EventCount * mockCperLen;
    std::vector<uint8_t> buffer1(read1Size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors1(read1EventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &read1Size, buffer1.data(), &read1EventCount, descriptors1.data(), nullptr));
    EXPECT_EQ(3u, read1EventCount);
    EXPECT_EQ(3u * mockCperLen, read1Size);

    // Verify first 3 events
    EXPECT_EQ(0x03u, descriptors1[0].address.bus); // Event 0: dev=0000:03:00.0
    EXPECT_EQ(0xf1, descriptors1[0].uuid.id[0]);   // fru_id starts with f1
    EXPECT_EQ(2750484567000ULL, descriptors1[0].timestamp);

    EXPECT_EQ(0x04u, descriptors1[1].address.bus); // Event 1: dev=0000:04:00.0
    EXPECT_EQ(0xf2, descriptors1[1].uuid.id[0]);   // fru_id starts with f2
    EXPECT_EQ(2750507923000ULL, descriptors1[1].timestamp);

    EXPECT_EQ(0x05u, descriptors1[2].address.bus); // Event 2: dev=0000:05:00.0
    EXPECT_EQ(0xf3, descriptors1[2].uuid.id[0]);   // fru_id starts with f3
    EXPECT_EQ(2750555466000ULL, descriptors1[2].timestamp);

    // Second read: request 1 event
    uint32_t read2EventCount = 1;
    uint32_t read2Size = read2EventCount * mockCperLen;
    std::vector<uint8_t> buffer2(read2Size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors2(read2EventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &read2Size, buffer2.data(), &read2EventCount, descriptors2.data(), nullptr));
    EXPECT_EQ(1u, read2EventCount);
    EXPECT_EQ(1u * mockCperLen, read2Size);

    // Verify event 3
    EXPECT_EQ(0x04u, descriptors2[0].address.bus); // Event 3: dev=0000:04:00.0
    EXPECT_EQ(0xf4, descriptors2[0].uuid.id[0]);   // fru_id starts with f4
    EXPECT_EQ(2750576952000ULL, descriptors2[0].timestamp);

    // Third read: request 4 events
    uint32_t read3EventCount = 4;
    uint32_t read3Size = read3EventCount * mockCperLen;
    std::vector<uint8_t> buffer3(read3Size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors3(read3EventCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &read3Size, buffer3.data(), &read3EventCount, descriptors3.data(), nullptr));
    EXPECT_EQ(4u, read3EventCount);
    EXPECT_EQ(4u * mockCperLen, read3Size);

    // Verify events 4-7
    EXPECT_EQ(0x03u, descriptors3[0].address.bus); // Event 4: dev=0000:03:00.0
    EXPECT_EQ(0xf5, descriptors3[0].uuid.id[0]);   // fru_id starts with f5
    EXPECT_EQ(2750594695000ULL, descriptors3[0].timestamp);

    EXPECT_EQ(0x05u, descriptors3[1].address.bus); // Event 5: dev=0000:05:00.0
    EXPECT_EQ(0xf6, descriptors3[1].uuid.id[0]);   // fru_id starts with f6
    EXPECT_EQ(2750616417000ULL, descriptors3[1].timestamp);

    EXPECT_EQ(0x07u, descriptors3[2].address.bus); // Event 6: dev=0000:07:00.0
    EXPECT_EQ(0xf7, descriptors3[2].uuid.id[0]);   // fru_id starts with f7
    EXPECT_EQ(2750634843000ULL, descriptors3[2].timestamp);

    EXPECT_EQ(0x08u, descriptors3[3].address.bus); // Event 7: dev=0000:08:00.0
    EXPECT_EQ(0xf8, descriptors3[3].uuid.id[0]);   // fru_id starts with f8
    EXPECT_EQ(2750657681000ULL, descriptors3[3].timestamp);

    // Fourth read: request remaining events (should be 0 since we've read all 8)
    uint32_t read4EventCount = 10; // Request more than available
    uint32_t read4Size = read4EventCount * mockCperLen;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &read4Size, nullptr, &read4EventCount, nullptr, nullptr));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

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
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount) << "A line cut short by EAGAIN must not be reported as a record";
    EXPECT_EQ(0u, size);

    // The remainder arrives; the retained prefix must be prepended so the record survives intact.
    pipeData += mockCperEvent1.substr(splitPos);

    size = mockCperLen;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

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
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);

    // The prefix can never be completed, so it must have been dropped rather than retained: if it were
    // still buffered it would be spliced onto the next line and swallow that record too.
    pipeData += mockCperEvent1;
    MockTraceFsApiWithData::drainedErrno = 0; // Stream is healthy again

    size = mockCperLen;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount) << "A discarded prefix must not corrupt the next complete record";
    EXPECT_EQ(mockCperLen, size);
}

namespace {
struct CorruptDataFactories {
    static std::unique_ptr<TraceFsApi> makeTruncatedEvent() {
        std::string data = mockCperEvent1;
        if (!data.empty() && data.back() == '\n') {
            data.pop_back();
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
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

    static std::unique_ptr<TraceFsApi> makeEventWithoutDev() {
        std::string data = mockCperEvent1;
        size_t pos = data.find("dev=0000:13:00.0");
        if (pos != std::string::npos) {
            data.erase(pos, 17);
        }
        return std::make_unique<MockTraceFsApiWithData>(false, false, data);
    }
};
} // anonymous namespace

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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should still process the incomplete line
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(mockCperLen, size);
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount); // Only event2 extracted
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Graceful: Record still processed with zero BDF
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, descriptors[0].address.domain);
    EXPECT_EQ(0u, descriptors[0].address.bus);
    EXPECT_EQ(0u, descriptors[0].address.device);
    EXPECT_EQ(0u, descriptors[0].address.function);
}

TEST_F(SysmanInfoLogFixture, GivenInfologHandleBufferTooSmallForOneRecordThenNothingIsReturnedAndDataStaysAvailable) {
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 100; // Too small for 532-byte record
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(mockCperLen);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(0u, eventCount); // No records fit
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.hasDataToRead);

    size = mockCperLen;
    eventCount = 10;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(1u, eventCount);
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 0;
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // pBuffer=NULL, pDescriptors!=NULL triggers query mode
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, descriptors.data(), nullptr));
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 2000;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    // Graceful: Returns whatever was extracted before fgets failed
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 10;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(10);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_NE(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenGlobalInstanceWhenCreatingASecondGlobalInstanceThenBothAreIndependent) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    zes_intel_info_log_instance_handle_t hInstance2 = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance2));
    EXPECT_NE(hInstance, hInstance2);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance2));
}

TEST_F(SysmanInfoLogFixture, GivenValidBufferSizeWhenCreatingInstanceThenRequestIsSplitAcrossPerCpuBuffersAndAppliedTotalIsReported) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    MockPerCpuDirBackup perCpuDirBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSize = 1024u;
    const size_t expectedPerCpuBufferSize = bufferSize / MockPerCpuDir::mockPerCpuBufferCount;

    auto desc = makeInstanceDesc(&bufferSize);
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(expectedPerCpuBufferSize, PublicTraceFsApi::lastSetBufferSize);
    EXPECT_EQ(-1, PublicTraceFsApi::lastSetBufferSizeCpu);
    EXPECT_NE(std::string::npos, MockPerCpuDir::openedPath.find("/per_cpu"));
    EXPECT_EQ(1u, MockPerCpuDir::closedirCallCount);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize), bufferSize);
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));

    EXPECT_EQ(2u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(static_cast<size_t>(MockTraceFsOsLibrary::mockBufferSize), PublicTraceFsApi::lastSetBufferSize);
}

TEST_F(SysmanInfoLogFixture, GivenZeroBufferSizeWhenCreatingInstanceThenBufferIsLeftAtItsCurrentSizeAndThatSizeIsReported) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    MockPerCpuDirBackup perCpuDirBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSize = 0u;
    auto desc = makeInstanceDesc(&bufferSize);
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
    // Nothing was resized, and the size the buffer already had is what the descriptor reports back.
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize), bufferSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPerCpuDirectoryCannotBeScannedWhenCreatingInstanceWithABufferSizeThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    MockPerCpuDirBackup perCpuDirBackup(true);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSize = 1024u;
    auto desc = makeInstanceDesc(&bufferSize);
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsInstanceCreationFailsWhenCreatingInstanceThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        mockApi->mockInstanceCreateReturnsNull = true; // Use correct member name
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], instanceName, &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeNotOpenedWhenCallingInfoLogReadWithMetaDataThenErrorIsReturned) {
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);
    ASSERT_EQ(-1, instance.getTracePipeFd());

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, instance.readWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeNotOpenedWhenCallingInfoLogPeekWithMetaDataThenRecordsAreStillReturned) {
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);
    ASSERT_EQ(-1, instance.getTracePipeFd());

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(mockCperLen, size);
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_EQ(ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE, descriptors[0].recordType);

    uint32_t secondSize = 1024;
    uint32_t secondCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &secondSize, buffer.data(), &secondCount, descriptors.data(), nullptr));
    EXPECT_EQ(mockCperLen, secondSize);
    EXPECT_EQ(1u, secondCount);
}

TEST_F(SysmanInfoLogFixture, GivenCperEventsWithDifferentSeveritiesWhenPeekingWithMetadataThenRecordTypeReflectsTheSeverity) {
    const std::vector<std::pair<const std::string *, zes_intel_info_log_record_type_exp_t>> severityToRecordType = {
        {&mockCperEventWithRecoverableSeverity, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE},
        {&mockCperEventWithFatalSeverity, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_FATAL},
        {&mockCperEventWithCorrectedSeverity, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED},
        {&mockCperEventWithInformationalSeverity, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_INFORMATIONAL},
        {&mockCperEventWithUnrecognizedSeverity, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN},
        {&mockCperEventWithNonNumericSeverity, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN},
        {&mockSmallCperTraceEvent, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN}};

    for (const auto &[traceData, expectedRecordType] : severityToRecordType) {
        MockTraceFsApiWithData traceFsApi(false, false, *traceData);
        LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

        uint32_t size = 1024;
        uint32_t eventCount = 1;
        std::vector<uint8_t> buffer(size);
        std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
        ASSERT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
        ASSERT_EQ(1u, eventCount);
        EXPECT_EQ(expectedRecordType, descriptors[0].recordType);
    }
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(0ULL, descriptors[0].timestamp); // Should be 0 when xe_error_cper marker not found
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
}

TEST_F(SysmanInfoLogFixture, GivenUuidWithInvalidFormatWhenParsingThenParseUuidFailsGracefully) {
    // fru_id field with invalid hex characters
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>(mockCperEventWithNonHexFruId);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // Should succeed but extract 0 events because line is not a CPER line
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 0;
    uint32_t eventCount = 0;
    // Query mode (pBuffer is null) - will call countCperRecordsAndSize
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, nullptr, nullptr));
    EXPECT_EQ(0u, eventCount); // Should be 0 because cper_len=0 is skipped
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenInstanceGetFileReturnsNullWhenCreatingNamedInstanceThenErrorIsReturned) {
    // Mock that returns nullptr for traceFsInstanceGetFile
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failGetFile = true; // Fail to get trace_pipe path
        return mockApi;
    });

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    // Should fail because traceFsInstanceGetFile returns nullptr
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], instanceName, &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeNotOpenedForNamedInstanceWhenReadingThenErrorIsReturned) {
    // The trace_pipe path is resolved, but every open of it fails
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
    });

    // Mock open of the trace_pipe to always fail. Opening the instance directory, which is how a named
    // instance claims ownership of the tracefs instance, still has to work for the read to be reached.
    auto mockOpenFail = [](const char *pathname, int flags) -> int {
        if (pathname && std::string(pathname).find("trace_pipe") != std::string::npos) {
            return -1;
        }
        return MockTraceFsApiWithData::mockSysCallsOpen(pathname, flags);
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, mockOpenFail);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "test_instance";
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    // Enable will fail because open fails
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], instanceName, &desc, &hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 0;
    uint32_t eventCount = 0;
    // Query mode - will fail because trace file read returns nullptr
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, nullptr, nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPreExistingNamedInstanceOwnedByAnotherCollectionInstanceWhenCreatingInstanceThenHandleObjectInUseIsReturnedAndInstanceIsNotDestroyed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    // The advisory lock on the instance directory is held elsewhere, which is what identifies the
    // instance as one this API is already collecting from.
    VariableBackup<int> flockRetValBackup(&NEO::SysCalls::flockRetVal, -1);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);

    // The owner is still collecting from it, so it is released without being destroyed, and the
    // descriptor opened to attempt the claim is not leaked.
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
    EXPECT_EQ(1, MockTraceFsApiWithData::instanceDirOpenCallCount);
    EXPECT_EQ(1, MockTraceFsApiWithData::instanceDirCloseCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPreExistingNamedInstanceNotOwnedByAnyCollectionInstanceWhenCreatingInstanceThenItIsReusedAndLeftInPlaceOnDelete) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // The instance pre-exists but nothing holds its lock, so it was provisioned outside of this API
    // and is collected from rather than refused.
    auto hInstance = createInfoLogInstance(infoLogHandles[0], "my_instance");
    EXPECT_EQ(1, MockTraceFsApiWithData::instanceDirOpenCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::instanceDirCloseCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
    // Deleting the collection instance releases the lock, which is what closing the directory does.
    EXPECT_EQ(1, MockTraceFsApiWithData::instanceDirCloseCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInstanceDirectoryCannotBeOpenedWhenCreatingNamedInstanceThenErrnoIsReportedAndInstanceIsNotDestroyed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        MockTraceFsApiWithData::simulateInstanceDirOpenFailure = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    // The mock reports EACCES, which is what the caller is told about.
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::instanceDirCloseCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenNoTracefsInstancesDirectoryWhenCreatingNamedInstanceThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutInstancesDir);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);
    EXPECT_EQ(0, MockTraceFsApiWithData::instanceDirOpenCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenUnnamedInstanceWhenCreatingInstanceThenNoOwnershipOfATracefsInstanceIsClaimed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    VariableBackup<int> flockCalledBackup(&NEO::SysCalls::flockCalled, 0);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    // Collection from the global tracefs buffer creates no tracefs instance, so there is nothing to
    // own and no other consumer to exclude.
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);
    EXPECT_EQ(0, MockTraceFsApiWithData::instanceDirOpenCallCount);
    EXPECT_EQ(0, NEO::SysCalls::flockCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(0, MockTraceFsApiWithData::instanceDirCloseCallCount);
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenIncompleteFinalLineNotFittingInBufferWhenReadingWithMetaDataThenTheRecordIsHeldBack) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 2;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
    EXPECT_TRUE(readStatus.hasDataToRead);
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));

    uint32_t size = 0;
    uint32_t eventCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, nullptr, nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(1u, MockTraceFsApiWithConfigurableBehavior::traceReadCallCount);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, MockTraceFsApiWithConfigurableBehavior::traceReadInstance);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenPeekingInfoLogThenTraceFileIsReadFromThatInstance) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failTraceFileRead = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));

    uint32_t size = 1024u;
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, peekInfoLogData(hInstance, &size, buffer.data()));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(5058240000000ULL, descriptors[0].timestamp);
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenNewlyCreatedNamedInstanceWhenDeletingInstanceThenInstanceIsDestroyedAndFreed) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(0u, MockTraceFsApiWithData::instanceFreeCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(0, MockTraceFsApiWithData::eventDisableCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenSetBufferSizeFailsWhenCreatingNamedInstanceThenErrorIsReturnedAndInstanceIsCleanedUp) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->setBufferSizeReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    MockPerCpuDirBackup perCpuDirBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSize = 1024u;
    auto desc = makeInstanceDesc(&bufferSize);
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceDestroyCallCount);
    EXPECT_EQ(1u, MockTraceFsApiWithData::instanceFreeCallCount);
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferSizeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenBufferFullHoldsBackAnOversizedRecordWhenReadingAgainThenItIsReturnedAheadOfTheFollowingRecord) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSmallCperTraceEvent, false, &mockLargerThenSmallCperTracePipeEvents);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithBadCperData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithBadCperData::mockSysCallsClose);
    MockBadCperStdioBackup stdioBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    uint32_t size = 2u;
    std::vector<uint8_t> buffer(1024u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(0u, size);

    size = 1024u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(6u, size);
    EXPECT_EQ(0xAB, buffer[0]);
    EXPECT_EQ(0xCD, buffer[1]);
    EXPECT_EQ(0xEF, buffer[2]);
    EXPECT_EQ(0x01, buffer[3]);
    EXPECT_EQ(0x12, buffer[4]);
    EXPECT_EQ(0x34, buffer[5]);
}

TEST_F(SysmanInfoLogFixture, GivenFirstTraceFsPathFailsWhenCreatingInstanceThenFallsBackToSecondPathAndReadSucceeds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>(false, true);
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
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    EXPECT_EQ(2, MockTraceFsApiWithData::openCallCount);

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(mockCperLen, size);

    for (uint32_t i = 0; i < expectedCper1Bytes.size(); i++) {
        EXPECT_EQ(expectedCper1Bytes[i], buffer[i]);
    }
}

TEST_F(SysmanInfoLogFixture, GivenInstanceCreatedWhenCreatingASecondInstanceThenEachOneOpensAndClosesItsOwnTracePipe) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> mockReadBackup(&NEO::SysCalls::sysCallsRead, MockTraceFsApiWithData::mockSysCallsRead);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto hFirstInstance = createInfoLogInstance(infoLogHandles[0]);
    ASSERT_EQ(1, MockTraceFsApiWithData::openCallCount);
    ASSERT_EQ(1u, getCperTracePipeFdCount());

    auto hSecondInstance = createInfoLogInstance(infoLogHandles[0]);
    EXPECT_EQ(2, MockTraceFsApiWithData::openCallCount);
    EXPECT_EQ(1u, getCperTracePipeFdCount());
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hFirstInstance));
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hSecondInstance));
    EXPECT_EQ(2, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(0u, getCperTracePipeFdCount());
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenInstanceWithoutAnOpenTracePipeWhenReadingWithMetaDataThenNotAvailableIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto nonCperFormat = static_cast<zes_intel_info_log_format_exp_t>(ZES_INTEL_INFO_LOG_FORMAT_CPER + 1);
    LinuxInfoLogImp infoLogImp(nonCperFormat);

    auto desc = makeInstanceDesc();
    std::unique_ptr<OsInfoLogInstance> pOsInfoLogInstance;
    ASSERT_EQ(ZE_RESULT_SUCCESS, infoLogImp.createInstance(nullptr, &desc, pOsInfoLogInstance));
    ASSERT_NE(nullptr, pOsInfoLogInstance.get());
    ASSERT_EQ(-1, pOsInfoLogInstance->getTracePipeFd());

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    uint32_t recordCount = maxRecordsPerRead;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(recordCount);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE,
              pOsInfoLogInstance->readWithMetadata(noTimeout, &size, buffer.data(), &recordCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, recordCount);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenCreatingAndDeletingItThenWakeWatermarkIsProgrammedOnThatInstance) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, PublicTraceFsApi::lastSetBufferPercentInstance);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, PublicTraceFsApi::lastSetBufferPercentInstance);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWithABufferSizeWhenCreatingInstanceThenItsOwnPerCpuDirectoryIsScannedAndWakeWatermarkIsStillForcedToZero) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    MockPerCpuDirBackup perCpuDirBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSize = 1002u;
    auto desc = makeInstanceDesc(&bufferSize);
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));

    EXPECT_EQ(std::string(MockTraceFsOsLibrary::mockTraceDir) + "/per_cpu", MockPerCpuDir::openedPath);
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(251u, PublicTraceFsApi::lastSetBufferSize);
    EXPECT_EQ(-1, PublicTraceFsApi::lastSetBufferSizeCpu);
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, PublicTraceFsApi::lastSetBufferPercentInstance);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstancePerCpuPathIsUnavailableWhenCreatingInstanceWithABufferSizeThenErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithConfigurableBehavior>();
        mockApi->failGetFile = true;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> mockAccessBackup(&NEO::SysCalls::sysCallsAccess, MockTraceFsApiWithData::mockSysCallsAccessWithoutPreExistingInstance);
    MockPerCpuDirBackup perCpuDirBackup;

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t bufferSize = 1024u;
    auto desc = makeInstanceDesc(&bufferSize);
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], "my_instance", &desc, &hInstance));
    EXPECT_TRUE(MockPerCpuDir::openedPath.empty());
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenInstanceCreatedWhenDeletingInstanceThenPreviousTraceFsWakeWatermarkIsRestored) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);
    ASSERT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    ASSERT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
}

TEST_F(SysmanInfoLogFixture, GivenNoInstanceCreatedWhenDriverIsDestroyedThenTraceFsStateIsLeftUntouched) {
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

TEST_F(SysmanInfoLogFixture, GivenInstanceStillAliveWhenDriverIsDestroyedThenCollectionIsStoppedAndTraceFsStateIsRestored) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    createInfoLogInstance(infoLogHandles[0]);
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

TEST_F(SysmanInfoLogFixture, GivenInfoLogFormatIsNotCperWhenCreatingInstanceThenTracingIsTurnedOnWithoutOpeningTracePipe) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);

    auto nonCperFormat = static_cast<zes_intel_info_log_format_exp_t>(ZES_INTEL_INFO_LOG_FORMAT_CPER + 1);
    LinuxInfoLogImp infoLogImp(nonCperFormat);

    auto desc = makeInstanceDesc();
    std::unique_ptr<OsInfoLogInstance> pOsInfoLogInstance;
    EXPECT_EQ(ZE_RESULT_SUCCESS, infoLogImp.createInstance(nullptr, &desc, pOsInfoLogInstance));
    EXPECT_NE(nullptr, pOsInfoLogInstance.get());

    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);
    EXPECT_EQ(0, MockTraceFsApiWithData::openCallCount);
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkAlreadyZeroWhenCreatingInstanceThenItIsNotRewritten) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->getBufferPercentReturnValue = 0;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkCannotBeProgrammedWhenCreatingInstanceThenCollectionStillSucceeds) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->setBufferPercentReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkCannotBeReadWhenCreatingInstanceThenItIsNotOverridden) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<MockTraceFsApiWithData>();
        mockApi->getBufferPercentReturnValue = -1;
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferPercentCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTraceFsWakeWatermarkRestoreFailsWhenDeletingInstanceThenDeleteStillSucceeds) {
    VariableBackup<decltype(PublicTraceFsApi::failSetBufferPercentOnCall)> failRestoreBackup(&PublicTraceFsApi::failSetBufferPercentOnCall, 2u);
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);
    ASSERT_EQ(1u, PublicTraceFsApi::setBufferPercentCallCount);
    ASSERT_EQ(0, PublicTraceFsApi::lastSetBufferPercent);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeOpenFailsWhenCreatingInstanceThenErrorIsReturnedAndTraceFsStateIsRolledBack) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithBadCperData>(mockSingleCperEventData, true);
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithBadCperData::mockSysCallsOpen);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(-1, getCperTracePipeFd());
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);
}

TEST_F(SysmanInfoLogFixture, GivenNullGlobalSysmanDriverWhenReadingInfoLogThenTheInstanceStillServesItsRecords) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> mockDupBackup(&NEO::SysCalls::sysCallsDup, MockTraceFsApiWithData::mockSysCallsDup);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFdopen)> mockFdopenBackup(&NEO::SysCalls::sysCallsFdopen, MockTraceFsApiWithData::mockSysCallsFdopen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFgets)> mockFgetsBackup(&NEO::SysCalls::sysCallsFgets, MockTraceFsApiWithData::mockSysCallsFgets);
    VariableBackup<decltype(NEO::SysCalls::sysCallsFclose)> mockFcloseBackup(&NEO::SysCalls::sysCallsFclose, MockTraceFsApiWithData::mockSysCallsFclose);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetvbuf)> mockSetvbufBackup(&NEO::SysCalls::sysCallsSetvbuf, MockTraceFsApiWithData::mockSysCallsSetvbuf);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    auto hInstance = createInfoLogInstance(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_NE(nullptr, L0::Sysman::globalSysmanDriver);

    auto *originalGlobalSysmanDriver = L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;

    uint32_t size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    std::vector<uint8_t> buffer(size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, readInfoLogData(hInstance, &size, buffer.data()));
    EXPECT_EQ(mockCperLen, size);

    size = static_cast<uint32_t>(MockTraceFsOsLibrary::mockBufferSize);
    uint32_t eventCount = 1;
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, size);
    EXPECT_EQ(0u, eventCount);

    L0::Sysman::globalSysmanDriver = originalGlobalSysmanDriver;
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullGlobalSysmanDriverWhenCreatingInstanceThenUninitializedIsReturnedAndTraceFsStateIsRolledBack) {
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(0, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(0, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferPercentCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    L0::Sysman::globalSysmanDriver = originalGlobalSysmanDriver;
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenNullGlobalSysmanDriverWhenDeletingInstanceThenDescriptorIsStillClosedAndSuccessIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);
    ASSERT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
    ASSERT_NE(nullptr, L0::Sysman::globalSysmanDriver);

    auto *originalGlobalSysmanDriver = L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(1, MockTraceFsApiWithData::closeCallCount);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, PublicTraceFsApi::lastSetBufferPercent);

    L0::Sysman::globalSysmanDriver = originalGlobalSysmanDriver;
    EXPECT_EQ(MockTraceFsApiWithData::mockTracePipeFd, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTracepointEnableStateReadsAsZeroWhenCreatingInstanceThenTracepointIsEnabledAgain) {
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(-1, getCperTracePipeFd());
}

TEST_F(SysmanInfoLogFixture, GivenTracingOnStateReadsAsZeroWhenCreatingInstanceThenTracingIsTurnedOnAndBackOffOnDelete) {
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
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));
    EXPECT_EQ(1u, MockTraceFsApiWithConfigurableBehavior::traceOnCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::traceOffCallCount);

    // Tracing was not already on before enabling, so disabling has to turn it back off.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(1, MockTraceFsApiWithData::traceOffCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenCreatingAnotherInstanceWithTheSameNameThenObjectInUseIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *instanceName = "named_instance";
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], instanceName, &desc, &hInstance));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    auto secondDesc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hSecondInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], instanceName, &secondDesc, &hSecondInstance));
    EXPECT_EQ(nullptr, hSecondInstance);
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], instanceName, &secondDesc, &hSecondInstance));
    EXPECT_EQ(2, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hSecondInstance));
}

TEST_F(SysmanInfoLogFixture, GivenNamedInstanceWhenCreatingADifferentNamedInstanceThenBothAreIndependent) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    const char *firstInstanceName = "instance_a";
    auto firstDesc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hFirstInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], firstInstanceName, &firstDesc, &hFirstInstance));
    EXPECT_EQ(1, MockTraceFsApiWithData::openCallCount);

    const char *secondInstanceName = "instance_b";
    auto secondDesc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hSecondInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], secondInstanceName, &secondDesc, &hSecondInstance));
    EXPECT_NE(hFirstInstance, hSecondInstance);
    EXPECT_EQ(2, MockTraceFsApiWithData::openCallCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hFirstInstance));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hSecondInstance));
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // The first read hands out a zero length line, which must not be mistaken for a complete record.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xABu, buffer[0]);
    EXPECT_EQ(0xCDu, buffer[1]);
    EXPECT_EQ(2u, descriptors[0].lengthOfData);
    EXPECT_EQ(5058247549000ULL, descriptors[0].timestamp);
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

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hInstance));

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size, 0);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(1);

    // The overlong line is abandoned on the third read, so the injected empty line lands inside the
    // resynchronization loop, where it must not be treated as the end of the discarded record.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(4u, MockTraceFsApiWithConfigurableBehavior::emptyLineOnFgetsCall);
    EXPECT_GT(MockTraceFsApiWithConfigurableBehavior::fgetsCallCount, 4u);
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xABu, buffer[0]);
    EXPECT_EQ(0xCDu, buffer[1]);
    EXPECT_EQ(2u, descriptors[0].lengthOfData);
    EXPECT_EQ(5058247549000ULL, descriptors[0].timestamp);
}

TEST_F(SysmanInfoLogFixture, GivenPerCpuStatsReportLostRecordsWhenPeekingWithMetaDataThenTheirSumIsReportedAsDropped) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({MockPerCpuStats::makeBlob(3, 0), MockPerCpuStats::makeBlob(0, 5),
                                             MockPerCpuStats::makeBlob(2, 4, 100), MockPerCpuStats::makeBlob(0, 0)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(1u, eventCount);

    // 'overrun' and 'dropped events' are summed across the per-CPU buffers, while 'commit overrun'
    // counts records lost to nested writes rather than to an overflow and is left out.
    EXPECT_EQ(14u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);

    ASSERT_EQ(static_cast<size_t>(MockPerCpuDir::mockPerCpuBufferCount), MockPerCpuStats::filesRead.size());
    EXPECT_EQ("per_cpu/cpu0/stats", MockPerCpuStats::filesRead[0]);
    EXPECT_EQ("per_cpu/cpu3/stats", MockPerCpuStats::filesRead[3]);
}

TEST_F(SysmanInfoLogFixture, GivenAlreadyReportedDropsWhenPeekingAgainThenOnlyTheNewOnesAreReported) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({MockPerCpuStats::makeBlob(7, 0), MockPerCpuStats::makeBlob(0, 0),
                                             MockPerCpuStats::makeBlob(0, 0), MockPerCpuStats::makeBlob(0, 0)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(7u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);

    // Two more records lost since that peek: the kernel counters are cumulative, so only the
    // difference is reported.
    MockPerCpuStats::blobs[0] = MockPerCpuStats::makeBlob(9, 0);

    size = 1024;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(2u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);

    // The number of per-CPU buffers cannot change while the instance is alive, so it is scanned once.
    EXPECT_EQ(1u, MockPerCpuDir::closedirCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPerCpuStatsCountersRestartedWhenPeekingAgainThenNoDropsAreReported) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({MockPerCpuStats::makeBlob(12, 0), MockPerCpuStats::makeBlob(0, 0),
                                             MockPerCpuStats::makeBlob(0, 0), MockPerCpuStats::makeBlob(0, 0)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(12u, readStatus.droppedRecordCount);

    // Resizing a buffer, or clearing it through tracefs, restarts the kernel counters. The reported
    // total has to follow them back down instead of underflowing into a huge count.
    MockPerCpuStats::blobs[0] = MockPerCpuStats::makeBlob(1, 0);

    size = 1024;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);
}

TEST_F(SysmanInfoLogFixture, GivenSomePerCpuStatsFilesCannotBeReadWhenPeekingWithMetaDataThenTheCountIsReportedAsInvalid) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({"", MockPerCpuStats::makeBlob(6, 0), "", MockPerCpuStats::makeBlob(0, 1)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));

    // The 7 records the readable buffers report are not the number of records lost, because the two
    // buffers which could not be read may have lost any number of them. Reporting the sum would tell
    // the caller a wrong count, so no count is reported and the read is still not failed over it.
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_FALSE(readStatus.isDroppedRecordCountValid);

    // The readable buffers are still visited, so a later read can report the loss once all of the
    // counters can be read.
    EXPECT_EQ(static_cast<size_t>(MockPerCpuDir::mockPerCpuBufferCount), MockPerCpuStats::filesRead.size());
}

TEST_F(SysmanInfoLogFixture, GivenPerCpuStatsBecomeReadableAgainWhenPeekingAgainThenTheLossOfBothIntervalsIsReported) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({"", MockPerCpuStats::makeBlob(0, 0),
                                             MockPerCpuStats::makeBlob(0, 0), MockPerCpuStats::makeBlob(0, 0)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_FALSE(readStatus.isDroppedRecordCountValid);

    // A read which reports no count leaves the baseline where it was, so the loss it could not report
    // is reported in full by the first read which can read all of the counters again.
    MockPerCpuStats::blobs[0] = MockPerCpuStats::makeBlob(5, 0);

    size = 1024;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(5u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);
}

TEST_F(SysmanInfoLogFixture, GivenPerCpuDirectoryCannotBeScannedWhenPeekingWithMetaDataThenNoDropsAreReported) {
    MockPerCpuDirBackup perCpuDirBackup(true);
    MockPerCpuStatsBackup perCpuStatsBackup({MockPerCpuStats::makeBlob(9, 9)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));

    // Without the number of per-CPU buffers there is nothing to read the counters from, so no count
    // is reported rather than 0 being reported as if nothing had been lost.
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_FALSE(readStatus.isDroppedRecordCountValid);
    EXPECT_TRUE(MockPerCpuStats::filesRead.empty());
}

TEST_F(SysmanInfoLogFixture, GivenUnparsableDropCounterWhenPeekingWithMetaDataThenTheCountIsReportedAsInvalid) {
    std::string malformedStats = "entries 12\n";        // no separator at all
    malformedStats += "overrun: not-a-number\n";        // drop counter that cannot be parsed
    malformedStats += "commit overrun: 5\n";            // 'overrun' only as a suffix of the field name
    malformedStats += "oldest event ts: 5058.247549\n"; // field carrying a separator of its own
    malformedStats += "dropped events: 8 unexpected\n"; // trailing text after the count
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({malformedStats, MockPerCpuStats::makeBlob(0, 0),
                                             MockPerCpuStats::makeBlob(0, 0), MockPerCpuStats::makeBlob(0, 0)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));

    // 'dropped events' parses as 8 and the trailing text is ignored, but 'overrun' carries a value
    // which cannot be read at all, so 8 is not the number of records this buffer lost and no count is
    // reported for the interval.
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_FALSE(readStatus.isDroppedRecordCountValid);
}

TEST_F(SysmanInfoLogFixture, GivenPerCpuStatsWithoutDropCountersWhenPeekingWithMetaDataThenTheCountIsReportedAsInvalid) {
    // A stats layout carrying none of the counters this driver sums, which is what an older or a newer
    // kernel could report. Nothing was parsed, so nothing is known about the records lost, and 0 is
    // not reported as if the counters had said none were.
    std::string statsWithoutDropCounters = "entries: 12\n";
    statsWithoutDropCounters += "commit overrun: 5\n";
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({statsWithoutDropCounters, statsWithoutDropCounters,
                                             statsWithoutDropCounters, statsWithoutDropCounters});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(0u, readStatus.droppedRecordCount);
    EXPECT_FALSE(readStatus.isDroppedRecordCountValid);
}

TEST_F(SysmanInfoLogFixture, GivenMoreDropsThanFitInTheReportedCountWhenPeekingWithMetaDataThenTheCountIsClamped) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({MockPerCpuStats::makeBlob(UINT32_MAX, 1), MockPerCpuStats::makeBlob(UINT32_MAX, 1),
                                             MockPerCpuStats::makeBlob(UINT32_MAX, 1), MockPerCpuStats::makeBlob(UINT32_MAX, 1)});
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));

    // The counters are 64 bit wide, the reported count is not.
    EXPECT_EQ(UINT32_MAX, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);
}

TEST_F(SysmanInfoLogFixture, GivenRecordsLostBeforeCollectionStartsWhenReadingWithMetaDataThenOnlyLaterLossesAreReported) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);
    MockPerCpuDirBackup perCpuDirBackup;
    MockPerCpuStatsBackup perCpuStatsBackup({MockPerCpuStats::makeBlob(40, 0), MockPerCpuStats::makeBlob(0, 0),
                                             MockPerCpuStats::makeBlob(0, 0), MockPerCpuStats::makeBlob(0, 0)});

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    // Three records lost after collection started. The 40 the shared buffer had already lost by then
    // are not this instance's to report, so the counters are baselined when collection starts.
    MockPerCpuStats::blobs[0] = MockPerCpuStats::makeBlob(43, 0);

    uint32_t size = 0;
    uint32_t eventCount = 0;
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, nullptr, &eventCount, nullptr, &readStatus));
    EXPECT_EQ(3u, readStatus.droppedRecordCount);
    EXPECT_TRUE(readStatus.isDroppedRecordCountValid);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenLastFieldValueEndsWithTrailingSpaceWhenPeekingWithMetaDataThenFieldScanStopsAtEndOfLine) {
    // The snapshot path is used on purpose: getline() strips the trailing newline, so the value's
    // trailing space becomes the last character of the line and the field scan runs off the end of
    // the line instead of stopping at a field separator.
    MockTraceFsApiWithData traceFsApi(false, false, mockCperEventWithTrailingSpaceValue);
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(2u, size);
    EXPECT_EQ(0xABu, buffer[0]);
    EXPECT_EQ(0xCDu, buffer[1]);
}

TEST_F(SysmanInfoLogFixture, GivenBufferTooSmallForFirstRecordWhenPeekingWithMetaDataThenSizeLimitStopsExtractionWithNothingReturned) {
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    // The single record is larger than the buffer, so nothing is copied and the read stops on the
    // size limit rather than draining the snapshot.
    uint32_t size = 8;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);
}

TEST_F(SysmanInfoLogFixture, GivenMoreRecordsRequestedThanAvailableWhenPeekingWithMetaDataThenSnapshotIsReportedDrained) {
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    // Room for two records but only one in the snapshot, so the second getline() drains it. A drained
    // stop reports no more data available.
    uint32_t size = 1024;
    uint32_t eventCount = 2;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);
    zes_intel_info_log_read_status_exp_t readStatus = {};
    readStatus.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(noTimeout, &size, buffer.data(), &eventCount, descriptors.data(), &readStatus));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(mockCperLen, size);
    EXPECT_FALSE(readStatus.hasDataToRead);
}

TEST_F(SysmanInfoLogFixture, GivenZeroTimeoutWhenPeekingWithMetaDataThenTheDeadlineStopsExtractionWhileAFiniteTimeoutStillReads) {
    MockTraceFsApiWithData traceFsApi;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t size = 1024;
    uint32_t eventCount = 2;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);

    // A zero timeout puts the deadline in the past, so the very first loop iteration stops before any
    // record is read.
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(0u, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);

    // A generous but finite timeout never fires within the test. The snapshot is non-consuming, so the
    // same record is still there to be read out.
    size = 1024;
    eventCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.peekWithMetadata(largeTimeoutMs, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(mockCperLen, size);
}

TEST_F(SysmanInfoLogFixture, GivenZeroTimeoutWhenReadingWithMetaDataThenTheDeadlineStopsBeforeConsumingTracePipeWhileAFiniteTimeoutStillReads) {
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
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    uint32_t size = mockCperLen;
    uint32_t eventCount = 1;
    std::vector<uint8_t> buffer(size);
    std::vector<zes_intel_info_log_metadata_exp> descriptors(eventCount);

    // A zero timeout puts the deadline in the past, so extraction stops on the first loop iteration,
    // before a single line is pulled from trace_pipe.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, 0u, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(0u, eventCount);
    EXPECT_EQ(0u, size);

    // A generous but finite timeout never fires within the test, so the buffered record is read out.
    size = mockCperLen;
    eventCount = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, largeTimeoutMs, &size, buffer.data(), &eventCount, descriptors.data(), nullptr));
    EXPECT_EQ(1u, eventCount);
    EXPECT_EQ(mockCperLen, size);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenBufferSizeReadBackFailsAfterSizingWhenApplyingBufferConfigurationThenAppliedTotalIsReportedAsZero) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockTraceFsApiWithConfigurableBehavior traceFsApi;
    // The set succeeds but every buffer-size query reports -1, so the applied total cannot be read back.
    traceFsApi.failGetBufferSize = true;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t bufferSize = 4096u;
    auto desc = makeInstanceDesc(&bufferSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.applyBufferConfiguration(&desc));

    // A failed read-back is reported as 0 rather than the raw -1, which would reach the caller as
    // 0xffffffff.
    EXPECT_EQ(0u, bufferSize);
}

TEST_F(SysmanInfoLogFixture, GivenZeroBufferSizeWhenApplyingBufferConfigurationThenTheCurrentTotalIsReportedWithoutResizing) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockTraceFsApiWithConfigurableBehavior traceFsApi;
    // cpu -1 reports the total across the per-CPU buffers while a single cpu reports just its own, so
    // the value which comes back tells the two queries apart.
    traceFsApi.reportDistinctPerCpuAndTotalBufferSize = true;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t bufferSize = 0u;
    auto desc = makeInstanceDesc(&bufferSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.applyBufferConfiguration(&desc));

    // The buffer keeps the size it already had, and that total is what is reported back, so a caller
    // can read the size without changing it.
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(static_cast<uint32_t>(MockTraceFsApiWithConfigurableBehavior::mockTotalBufferSizeKb), bufferSize);
}

TEST_F(SysmanInfoLogFixture, GivenZeroBufferSizeAndBufferSizeCannotBeReadWhenApplyingBufferConfigurationThenSizeIsReportedAsZero) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockTraceFsApiWithConfigurableBehavior traceFsApi;
    // Every buffer-size query reports -1, so the size the buffer already has cannot be determined.
    traceFsApi.failGetBufferSize = true;
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

    uint32_t bufferSize = 0u;
    auto desc = makeInstanceDesc(&bufferSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.applyBufferConfiguration(&desc));

    // Still nothing is resized, and the unknown size is reported as 0 rather than the raw -1.
    EXPECT_EQ(0u, PublicTraceFsApi::setBufferSizeCallCount);
    EXPECT_EQ(0u, bufferSize);
}

TEST_F(SysmanInfoLogFixture, GivenBufferSizeRestoreFailsWhenTearingDownInstanceThenTheOverrideIsLeftInPlace) {
    MockPerCpuDirBackup perCpuDirBackup;
    MockTraceFsApiWithConfigurableBehavior traceFsApi;
    traceFsApi.reportDistinctPerCpuAndTotalBufferSize = true;
    // The sizing set (the first) succeeds; the restore set (the second, issued on teardown) fails.
    traceFsApi.failSetBufferSizeOnCall = 2;
    {
        LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, false, false);

        uint32_t bufferSize = 4096u;
        auto desc = makeInstanceDesc(&bufferSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, instance.applyBufferConfiguration(&desc));
        EXPECT_EQ(1u, PublicTraceFsApi::setBufferSizeCallCount);
    }
    // Teardown attempts the restore set. It fails, so the size stays overridden, but nothing throws
    // and the destructor completes.
    EXPECT_EQ(2u, PublicTraceFsApi::setBufferSizeCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenTracePipeCannotBeOpenedWhenStartingCollectionWithPreExistingTraceStateThenTheStateIsLeftUntouched) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, [](const char *, int) -> int {
        errno = ENOENT;
        return -1;
    });
    MockPerCpuDirBackup perCpuDirBackup;
    MockTraceFsApiWithData traceFsApi;
    // The tracepoint was already enabled and tracing already on before this instance started.
    LinuxInfoLogInstanceImp instance(&traceFsApi, ZES_INTEL_INFO_LOG_FORMAT_CPER, nullptr, "", false, true, true);

    MockTraceFsApiWithData::traceOffCallCount = 0;
    MockTraceFsApiWithData::eventDisableCallCount = 0;
    EXPECT_NE(ZE_RESULT_SUCCESS, instance.startCollection());

    // The failed startup must not tear down trace state it did not set up, so neither the tracepoint
    // nor tracing is turned off.
    EXPECT_EQ(0, MockTraceFsApiWithData::traceOffCallCount);
    EXPECT_EQ(0, MockTraceFsApiWithData::eventDisableCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenPropertyCaptureFailedAtInitWhenGettingPropertiesThenTheCaptureErrorIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    InfoLogImp infoLog(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    auto pMockOsInfoLog = std::make_unique<MockOsInfoLog>();
    pMockOsInfoLog->getPropertiesResult = ZE_RESULT_ERROR_UNKNOWN;
    infoLog.pOsInfoLog = std::move(pMockOsInfoLog);
    infoLog.init();

    // A failed capture at init is remembered and reported to every property query.
    zes_intel_info_log_properties_exp_t properties = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, infoLog.infoLogGetProperties(&properties));
}

TEST_F(SysmanInfoLogFixture, GivenPropertyCaptureFailedAtInitWhenCreatingInstanceThenTheCaptureErrorIsReturnedAndTheBackendIsNotAsked) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    InfoLogImp infoLog(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    auto pMockOsInfoLog = std::make_unique<MockOsInfoLog>();
    pMockOsInfoLog->getPropertiesResult = ZE_RESULT_ERROR_UNKNOWN;
    auto *pRawMockOsInfoLog = pMockOsInfoLog.get();
    infoLog.pOsInfoLog = std::move(pMockOsInfoLog);
    infoLog.init();

    // Without captured properties there is nothing to create a collection instance from, so the
    // capture error is reported instead of the properties being read as if they were valid.
    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, infoLog.infoLogCreateInstance(nullptr, &desc, &hInstance));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, infoLog.infoLogCreateInstance("named", &desc, &hInstance));
    EXPECT_EQ(nullptr, hInstance);
    EXPECT_EQ(0u, pRawMockOsInfoLog->createInstanceCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenNamedCollectionUnsupportedWhenCreatingNamedInstanceThenUnsupportedFeatureIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    InfoLogImp infoLog(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    auto pMockOsInfoLog = std::make_unique<MockOsInfoLog>();
    pMockOsInfoLog->isNamedInstancedCollectionSupported = false;
    infoLog.pOsInfoLog = std::move(pMockOsInfoLog);
    infoLog.init();

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, infoLog.infoLogCreateInstance("named", &desc, &hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenInstanceNotOwnedByThisInfoLogWhenDestroyingItThenInvalidNullHandleIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        return std::make_unique<MockTraceFsApiWithData>();
    });
    InfoLogImp infoLog(ZES_INTEL_INFO_LOG_FORMAT_CPER);
    // An instance this InfoLog never handed out is not in its bookkeeping.
    InfoLogInstanceImp strayInstance(&infoLog, nullptr, std::make_unique<MockOsInfoLogInstance>());

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, infoLog.destroyInstance(&strayInstance));
}

TEST_F(SysmanInfoLogFixture, GivenInstanceAlreadyTornDownWhenTearingDownAgainThenTheBackendIsToldOnlyOnce) {
    auto pMockOsInstance = std::make_unique<MockOsInfoLogInstance>();
    auto *pRawMockOsInstance = pMockOsInstance.get();
    InfoLogInstanceImp instance(nullptr, nullptr, std::move(pMockOsInstance));

    // The second teardown is a no-op: the backend is only torn down on the first call.
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.teardown());
    EXPECT_EQ(ZE_RESULT_SUCCESS, instance.teardown());
    EXPECT_EQ(1u, pRawMockOsInstance->teardownCallCount);
}

TEST_F(SysmanInfoLogFixture, GivenSysmanInitFromCoreWhenCallingInfoLogEntryPointsThenUnsupportedFeatureIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    // Sysman was brought up from core rather than as a standalone init, so every experimental
    // info-log entry point is refused up front on the init state alone.
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, true);

    uint32_t count = handleCount;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hNewInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hNewInstance));
    EXPECT_EQ(nullptr, hNewInstance);

    uint32_t size = mockCperLen;
    uint32_t recordCount = 1;
    std::vector<uint8_t> buffer(size);
    zes_intel_info_log_metadata_exp descriptor = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              zesIntelInfoLogInstancePeekWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

TEST_F(SysmanInfoLogFixture, GivenNeitherInitFlagSetWhenCallingInfoLogEntryPointsThenUninitializedIsReturned) {
    VariableBackup<decltype(LinuxInfoLogImp::createTraceFsApi)> createTraceFsApiBackup(&LinuxInfoLogImp::createTraceFsApi, []() -> std::unique_ptr<TraceFsApi> {
        auto mockApi = std::make_unique<PublicTraceFsApi>();
        mockApi->loadEntryPointsFromBase();
        return mockApi;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpenBackup(&NEO::SysCalls::sysCallsOpen, MockTraceFsApiWithData::mockSysCallsOpen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockCloseBackup(&NEO::SysCalls::sysCallsClose, MockTraceFsApiWithData::mockSysCallsClose);

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);
    auto hInstance = createInfoLogInstance(infoLogHandles[0]);

    // Neither init path ran, so the entry points report the driver as uninitialized on the state alone.
    VariableBackup<bool> sysmanInitFromCoreBackup(&L0::sysmanInitFromCore, false);
    VariableBackup<bool> sysmanOnlyInitBackup(&L0::Sysman::sysmanOnlyInit, false);

    uint32_t count = handleCount;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));

    auto desc = makeInstanceDesc();
    zes_intel_info_log_instance_handle_t hNewInstance = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogCreateInstanceExp(infoLogHandles[0], nullptr, &desc, &hNewInstance));
    EXPECT_EQ(nullptr, hNewInstance);

    uint32_t size = mockCperLen;
    uint32_t recordCount = 1;
    std::vector<uint8_t> buffer(size);
    zes_intel_info_log_metadata_exp descriptor = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED,
              zesIntelInfoLogInstanceReadWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED,
              zesIntelInfoLogInstancePeekWithMetadataExp(hInstance, noTimeout, &size, buffer.data(), &recordCount, &descriptor, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesIntelInfoLogInstanceDeleteExp(hInstance));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
