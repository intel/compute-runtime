/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

static constexpr uint32_t handleCount = 1u;
class SysmanInfoLogFixture : public ::testing::Test {
  protected:
    void SetUp() override {
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

    L0::Sysman::SysmanDriverHandle *driverHandle = nullptr;
    NEO::ExecutionEnvironment *execEnv = nullptr;
    const uint32_t numRootDevices = 1u;
};

TEST_F(SysmanInfoLogFixture, GivenValidDriverHandleWhenEnumeratingInfoLogsThenSuccessAndOneCountAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(1u, count);
}

TEST_F(SysmanInfoLogFixture, GivenDriverWhenEnumeratingInfoLogsThenHandleCreationSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);

    std::vector<zes_intel_info_log_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, handles.data()));

    auto hInfoLog = handles[0];
    EXPECT_NE(nullptr, hInfoLog);
}

TEST_F(SysmanInfoLogFixture, GivenRequestedInfoLogCountGreaterThanOneWhenEnumeratingInfoLogsThenOneHandleCountIsReturned) {

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);

    count = count + 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelDriverEnumInfoLogsExp(driverHandle->toHandle(), &count, nullptr));
    EXPECT_EQ(handleCount, count);
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingGetPropertiesApiThenSuccessIsReturned) {

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    zes_intel_info_log_properties_exp_t properties = {};
    properties.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesIntelInfoLogGetPropertiesExp(infoLogHandles[0], &properties));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingReadApiThenUnsupportedFeatureIsReturned) {

    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    uint32_t size = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogReadExp(infoLogHandles[0], &size, nullptr));
}

TEST_F(SysmanInfoLogFixture, GivenValidInfoLogHandleWhenCallingEnableApiThenUnsupportedFeatureIsReturned) {
    auto infoLogHandles = getInfoLogHandles(handleCount);
    ASSERT_NE(nullptr, infoLogHandles[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesIntelInfoLogEnableExp(infoLogHandles[0], true));
}

TEST_F(SysmanInfoLogFixture, GivenNullOsSysmanDriverWhenEnumeratingInfoLogsThenErrorIsReturned) {
    auto *pSysmanDriverHandleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    auto *originalOsSysmanDriver = pSysmanDriverHandleImp->pOsSysmanDriver;
    pSysmanDriverHandleImp->pOsSysmanDriver = nullptr;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pSysmanDriverHandleImp->enumInfoLogs(&count, nullptr));

    pSysmanDriverHandleImp->pOsSysmanDriver = originalOsSysmanDriver;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
