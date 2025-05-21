/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/pmu/mock_pmu.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include <cmath>

namespace L0 {
namespace Sysman {
namespace ult {

struct MockFsAccess : public L0::Sysman::FsAccessInterface {

    std::string mockReadValue = "";
    ze_result_t engineActiveTicksReadStatus = ZE_RESULT_SUCCESS;
    ze_result_t engineTotalTicksReadStatus = ZE_RESULT_SUCCESS;
    ze_result_t gtFileReadStatus = ZE_RESULT_SUCCESS;
    ze_result_t engineClassFileReadStatus = ZE_RESULT_SUCCESS;
    ze_result_t engineInstanceFileReadStatus = ZE_RESULT_SUCCESS;
    ze_result_t functionFileReadStatus = ZE_RESULT_SUCCESS;

    ze_result_t read(const std::string file, std::string &val) override {

        if (!mockReadValue.empty()) {
            val = mockReadValue;
        } else if ((engineActiveTicksReadStatus == ZE_RESULT_SUCCESS) && (file.find("engine-active-ticks") != std::string::npos)) {
            val = "events=0x02";
        } else if ((engineTotalTicksReadStatus == ZE_RESULT_SUCCESS) && (file.find("engine-total-ticks") != std::string::npos)) {
            val = "events=0x03";
        } else if ((gtFileReadStatus == ZE_RESULT_SUCCESS) && (file.find("gt") != std::string::npos)) {
            val = "config:0-7";
        } else if ((engineClassFileReadStatus == ZE_RESULT_SUCCESS) && (file.find("engine_class") != std::string::npos)) {
            val = "config:8-15";
        } else if ((engineInstanceFileReadStatus == ZE_RESULT_SUCCESS) && (file.find("engine_instance") != std::string::npos)) {
            val = "config:16-23";
        } else if ((functionFileReadStatus == ZE_RESULT_SUCCESS) && (file.find("function") != std::string::npos)) {
            val = "config:0-7";
        } else {
            val = mockReadValue;
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        return ZE_RESULT_SUCCESS;
    }

    MockFsAccess() = default;
};

struct SysmanXePmuFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockPmuInterface> pPmuInterface;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    MockFsAccess *pFsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccess = new MockFsAccess();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface = std::make_unique<MockPmuInterface>(pLinuxSysmanImp);
        pPmuInterface->pSysmanKmdInterface = pSysmanKmdInterface;
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    }
    void TearDown() override {
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndValidConfigEventFileWhenCallingGetConfigFromEventFileThenValidConfigValueAndSuccessIsReturned) {
    std::string_view eventFile = "sys/dev/xe/engine-active-ticks";
    uint64_t mockEventConfig = 2u;
    uint64_t config = UINT64_MAX;
    EXPECT_EQ(0, pPmuInterface->getConfigFromEventFile(eventFile, config));
    EXPECT_EQ(mockEventConfig, config);

    eventFile = "sys/dev/xe/engine-total-ticks";
    mockEventConfig = 3u;
    config = UINT64_MAX;
    EXPECT_EQ(0, pPmuInterface->getConfigFromEventFile(eventFile, config));
    EXPECT_EQ(mockEventConfig, config);
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndInvalidConfigEventFileWhenCallingGetConfigFromEventFileThenFailureIsReturned) {
    constexpr std::string_view eventFile = "sys/dev/xe/engine-ticks";
    uint64_t config = UINT64_MAX;
    EXPECT_EQ(-1, pPmuInterface->getConfigFromEventFile(eventFile, config));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndFsReadFailsForEventFileWhenCallingGetConfigFromEventFileThenFailureIsReturned) {
    constexpr std::string_view eventFile = "sys/dev/xe/engine-active-ticks";
    pFsAccess->engineActiveTicksReadStatus = ZE_RESULT_ERROR_UNKNOWN;
    uint64_t config = UINT64_MAX;
    EXPECT_EQ(-1, pPmuInterface->getConfigFromEventFile(eventFile, config));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndEventFileHasInvalidValueWhenCallingGetConfigFromEventFileThenFailureIsReturned) {
    constexpr std::string_view eventFile = "sys/dev/xe/engine-active-ticks";
    pFsAccess->mockReadValue = "invalidValue";
    uint64_t config = UINT64_MAX;
    EXPECT_EQ(-1, pPmuInterface->getConfigFromEventFile(eventFile, config));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndValidFormatConfigDirectoryNameWhenCallingGetConfigAfterFormatThenValidConfigValueAndSuccessIsReturned) {
    constexpr std::string_view formatDir = "/sys/dev/xe/format";
    uint64_t mockConfig = 1u;
    uint32_t mockEngineClass = 1u;
    uint32_t mockEngineInstance = 0u;
    uint32_t mockGt = 0u;
    uint64_t mockConfigAfterFormat = mockConfig | mockEngineClass << 8;
    EXPECT_EQ(0, pPmuInterface->getConfigAfterFormat(formatDir, mockConfig, mockEngineClass, mockEngineInstance, mockGt));
    EXPECT_EQ(mockConfigAfterFormat, mockConfig);
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndFsReadForGtFailsWhenCallingGetConfigAfterFormatThenFailureIsReturned) {
    constexpr std::string_view formatDir = "/sys/dev/xe/format";
    uint64_t mockConfig = 1u;
    uint32_t mockEngineClass = 1u;
    uint32_t mockEngineInstance = 0u;
    uint32_t mockGt = 0u;
    pFsAccess->gtFileReadStatus = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(-1, pPmuInterface->getConfigAfterFormat(formatDir, mockConfig, mockEngineClass, mockEngineInstance, mockGt));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndFsReadForEngineClassFailsWhenCallingGetConfigAfterFormatThenFailureIsReturned) {
    constexpr std::string_view formatDir = "/sys/dev/xe/format";
    uint64_t mockConfig = 1u;
    uint32_t mockEngineClass = 1u;
    uint32_t mockEngineInstance = 0u;
    uint32_t mockGt = 0u;
    pFsAccess->engineClassFileReadStatus = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(-1, pPmuInterface->getConfigAfterFormat(formatDir, mockConfig, mockEngineClass, mockEngineInstance, mockGt));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndFsReadForEngineInstanceFailsWhenCallingGetConfigAfterFormatThenFailureIsReturned) {
    constexpr std::string_view formatDir = "/sys/dev/xe/format";
    uint64_t mockConfig = 1u;
    uint32_t mockEngineClass = 1u;
    uint32_t mockEngineInstance = 0u;
    uint32_t mockGt = 0u;
    pFsAccess->engineInstanceFileReadStatus = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(-1, pPmuInterface->getConfigAfterFormat(formatDir, mockConfig, mockEngineClass, mockEngineInstance, mockGt));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndGtFileHasInvalidValueWhenCallingGetConfigAfterFormatThenFailureIsReturned) {
    constexpr std::string_view formatDir = "/sys/dev/xe/format";
    uint64_t mockConfig = 1u;
    uint32_t mockEngineClass = 1u;
    uint32_t mockEngineInstance = 0u;
    uint32_t mockGt = 0u;
    pFsAccess->mockReadValue = "invalidValue";
    EXPECT_EQ(-1, pPmuInterface->getConfigAfterFormat(formatDir, mockConfig, mockEngineClass, mockEngineInstance, mockGt));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndGtFileHasInvalidConfigFormatWhenCallingGetConfigAfterFormatThenFailureIsReturned) {
    constexpr std::string_view formatDir = "/sys/dev/xe/format";
    uint64_t mockConfig = 1u;
    uint32_t mockEngineClass = 1u;
    uint32_t mockEngineInstance = 0u;
    uint32_t mockGt = 0u;
    pFsAccess->mockReadValue = "config:32";
    EXPECT_EQ(-1, pPmuInterface->getConfigAfterFormat(formatDir, mockConfig, mockEngineClass, mockEngineInstance, mockGt));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndFunctionFileHasInvalidConfigValueWhenCallingGetPmuConfigsForVfThenFailureIsReturned) {
    constexpr std::string_view sysmanDeviceDir = "/sys/dev/xe/";
    uint64_t fnNumber = 1u;
    uint64_t activeTicksConfig = 2u;
    uint64_t totalTicksConfig = 3u;
    pFsAccess->mockReadValue = "config:0";
    EXPECT_EQ(-1, pPmuInterface->getPmuConfigsForVf(sysmanDeviceDir, fnNumber, activeTicksConfig, totalTicksConfig));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndFsReadForFunctionFileFailsWhenCallingGetPmuConfigsForVfThenFailureIsReturned) {
    constexpr std::string_view sysmanDeviceDir = "/sys/dev/xe/";
    uint64_t fnNumber = 1u;
    uint64_t activeTicksConfig = 2u;
    uint64_t totalTicksConfig = 3u;
    pFsAccess->functionFileReadStatus = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(-1, pPmuInterface->getPmuConfigsForVf(sysmanDeviceDir, fnNumber, activeTicksConfig, totalTicksConfig));
}

TEST_F(SysmanXePmuFixture, GivenPmuHandleAndValidFunctionFileContentsWhenCallingGetPmuConfigsForVfThenValidConfigsAndSuccessIsReturned) {
    constexpr std::string_view sysmanDeviceDir = "/sys/dev/xe/";
    constexpr uint64_t mockActiveTicksConfig = 3u;
    constexpr uint64_t mockTotalTicksConfig = 5u;
    uint64_t fnNumber = 1u;
    uint64_t activeTicksConfig = 2u;
    uint64_t totalTicksConfig = 4u;
    EXPECT_EQ(0, pPmuInterface->getPmuConfigsForVf(sysmanDeviceDir, fnNumber, activeTicksConfig, totalTicksConfig));
    EXPECT_EQ(mockActiveTicksConfig, activeTicksConfig);
    EXPECT_EQ(mockTotalTicksConfig, totalTicksConfig);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
