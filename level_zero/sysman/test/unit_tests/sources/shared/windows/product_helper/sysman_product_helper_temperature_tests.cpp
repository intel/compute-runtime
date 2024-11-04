/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/temperature/windows/sysman_os_temperature_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/windows/mock_temperature.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

std::vector<wchar_t> pmtInterface(pmtInterfaceName.begin(), pmtInterfaceName.end());
const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMap = {
    {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", {41, 1}},
     {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", {42, 1}}}};

class SysmanProductHelperTemperatureTest : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<TemperatureKmdSysManager> pKmdSysManager = nullptr;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new TemperatureKmdSysManager);
        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();
        auto pPmt = new PublicPlatformMonitoringTech(pmtInterface, pWddmSysmanImp->getSysmanProductHelper());
        pPmt->keyOffsetMap = dummyKeyOffsetMap;
        pWddmSysmanImp->pPmt.reset(pPmt);

        pSysmanDeviceImp->pTempHandleContext->handleList.clear();
    }
    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds, IsAtMostDg2) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenTempDomainsAreEnumeratedWhenCallingIsTempInitCompletedThenVerifyTempInitializationIsCompleted, IsAtMostDg2) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    EXPECT_EQ(true, pSysmanDeviceImp->pTempHandleContext->isTempInitDone());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenInvalidComponentCountWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds, IsAtMostDg2) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidTemperatureHandlesIsReturned, IsAtMostDg2) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidDeviceHandleWhenEnumeratingTemperatureSensorsOnIntegratedDeviceThenZeroCountIsReturned, IsAtMostDg2) {
    uint32_t count = 0;
    pKmdSysManager->isIntegrated = true;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTemperatureHandleWhenGettingTemperaturePropertiesAllowSetToTrueThenCallSucceeds, IsAtMostDg2) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    uint32_t sensorTypeIndex = 0;
    for (auto handle : handles) {
        zes_temp_properties_t properties;

        ze_result_t result = zesTemperatureGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_FALSE(properties.isCriticalTempSupported);
        EXPECT_FALSE(properties.isThreshold1Supported);
        EXPECT_FALSE(properties.isThreshold2Supported);
        EXPECT_EQ(properties.maxTemperature, pKmdSysManager->mockMaxTemperature);
        EXPECT_EQ(properties.type, pKmdSysManager->mockSensorTypes[sensorTypeIndex++]);
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingMemoryTemperatureThenValidTemperatureReadingsRetrieved, IsAtMostDg2) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_MEMORY], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempMemory));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingGPUTemperatureThenValidTemperatureReadingsRetrieved, IsAtMostDg2) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GPU], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGPU));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingGlobalTemperatureThenValidTemperatureReadingsRetrieved, IsAtMostDg2) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GLOBAL], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGlobal));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned, IsAtMostDg2) {
    auto pTemperatureImpMemory = std::make_unique<L0::Sysman::TemperatureImp>(pOsSysman, false, 0, ZES_TEMP_SENSORS_GLOBAL_MIN);
    auto pWddmTemperatureImp = static_cast<L0::Sysman::WddmTemperatureImp *>(pTemperatureImpMemory->pOsTemperature.get());
    double pTemperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&pTemperature));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenInvalidTemperatureTypeWhenCheckingIfTypeIsSupportedThenCallReturnsFalse, IsAtMostDg2) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GLOBAL_MIN);
    ASSERT_EQ(false, pWddmTemperatureImp->isTempModuleSupported());

    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GPU_MIN);
    ASSERT_EQ(false, pWddmTemperatureImp->isTempModuleSupported());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenInvalidTemperatureEscapeCallThenHandleCreationFails, IsAtMostDg2) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman);

    std::vector<uint32_t> requestId = {KmdSysman::Requests::Temperature::NumTemperatureDomains, KmdSysman::Requests::Temperature::CurrentTemperature};
    for (auto it = requestId.begin(); it != requestId.end(); it++) {
        pKmdSysManager->mockTemperatureFailure[*it] = 1;
        ASSERT_EQ(false, pWddmTemperatureImp->isTempModuleSupported());
        pKmdSysManager->mockTemperatureFailure[*it] = 0;
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingMemoryTemperatureThenValidTemperatureReadingsRetrieved, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 4;
        *static_cast<int *>(lpOutBuffer) = 23 | 512;
        return true;
    });
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_MEMORY], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempMemory));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingGPUTemperatureThenValidTemperatureReadingsRetrieved, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 4;
        *static_cast<int *>(lpOutBuffer) = 25;
        return true;
    });
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GPU], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGPU));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidTempHandleWhenGettingGlobalTemperatureThenValidTemperatureReadingsRetrieved, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 4;
        *static_cast<int *>(lpOutBuffer) = 25;
        return true;
    });
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GLOBAL], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(std::max(pKmdSysManager->mockTempGPU, pKmdSysManager->mockTempMemory)));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenNullPmtHandleWhenGettingGlobalTemperatureThenCallFails, IsBMG) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    pWddmSysmanImp->pPmt.reset(nullptr);
    double temperature;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GLOBAL], &temperature));
    EXPECT_EQ(temperature, 0);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenGettingGlobalTemperatureAndIoctlFailsThenCallsFails, IsBMG) {
    static uint32_t count = 2;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 42:
            *lpBytesReturned = 4;
            return count == 1 ? false : true;
        case 43:
            *lpBytesReturned = 4;
            return count == 2 ? false : true;
        default:
            return false;
        }
    });

    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;

    while (count) {
        ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GLOBAL], &temperature));
        count--;
    }
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenInvalidTempHandleWhenGettingGlobalTemperatureThenCallsFails, IsBMG) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_FORCE_UINT32);
    double temperature;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&temperature));
    EXPECT_EQ(temperature, 0);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenUnsupportedTemperatureTypeWhenCheckingForTempModuleSupportThenCallReturnsFalse, IsBMG) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GLOBAL_MIN);
    ASSERT_EQ(false, pWddmTemperatureImp->isTempModuleSupported());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenValidPmtHandleWhenCallingZesTemperatureGetStateAndIoctlFailsThenCallsFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 42:
            *lpBytesReturned = 4;
            return false;
        case 43:
            *lpBytesReturned = 4;
            return false;
        default:
            return false;
        }
    });
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    for (auto handle : handles) {
        ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesTemperatureGetState(handle, &temperature));
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
