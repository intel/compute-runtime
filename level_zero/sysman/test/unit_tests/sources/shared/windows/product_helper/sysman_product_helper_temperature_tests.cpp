/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/temperature/windows/sysman_os_temperature_imp.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/windows/mock_temperature.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <map>

namespace NEO {
namespace SysCalls {
extern DWORD getLastErrorResults[];
} // namespace SysCalls
} // namespace NEO

namespace L0 {
namespace Sysman {
namespace ult {

const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMap = {
    {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", {41, 1}},
     {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", {42, 1}}}};

class SysmanProductHelperTemperatureTest : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<TemperatureKmdSysManager> pKmdSysManager = nullptr;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    PublicPlatformMonitoringTech *pPmt = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new TemperatureKmdSysManager);
        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pPmt = new PublicPlatformMonitoringTech(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
        pPmt->keyOffsetMap = dummyKeyOffsetMap;
        pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
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
        zes_temp_properties_t properties = {};

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
    auto pTemperatureImpMemory = std::make_unique<L0::Sysman::TemperatureImp>(pOsSysman, false, 0, ZES_TEMP_SENSORS_GLOBAL_MIN, 0);
    auto pWddmTemperatureImp = static_cast<L0::Sysman::WddmTemperatureImp *>(pTemperatureImpMemory->pOsTemperature.get());
    double pTemperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&pTemperature));
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenInvalidTemperatureTypeWhenCheckingIfTypeIsSupportedThenCallReturnsFalse, IsAtMostDg2) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman, false, 0, 0);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GLOBAL_MIN);
    ASSERT_EQ(false, pWddmTemperatureImp->isTempModuleSupported());

    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GPU_MIN);
    ASSERT_EQ(false, pWddmTemperatureImp->isTempModuleSupported());
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenInvalidTemperatureEscapeCallThenHandleCreationFails, IsAtMostDg2) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman, false, 0, 0);

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
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman, false, 0, 0);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_FORCE_UINT32);
    double temperature;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&temperature));
    EXPECT_EQ(temperature, 0);
}

HWTEST2_F(SysmanProductHelperTemperatureTest, GivenUnsupportedTemperatureTypeWhenCheckingForTempModuleSupportThenCallReturnsFalse, IsBMG) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman, false, 0, 0);
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

constexpr uint32_t criPunitTelemetryIndex = 0;
constexpr uint32_t criOobmsmTelemetryIndex = 1;
constexpr uint32_t criAmbientTemperatureOffset = 22;
constexpr uint32_t criVrTemperatureBaseOffset = 56;
constexpr uint32_t criCompositeTemperatureOffset = 68;
constexpr uint32_t criSocTopDieTemperatureOffset = 32;
constexpr uint32_t criVramTemperatureOffset = 33;
constexpr uint32_t criVrTemperatureSensorCount = 4;
constexpr uint32_t criGpuBoardTemperatureSensorCount = 2;
constexpr uint32_t criTemperatureHandleCount = 12;
constexpr uint32_t criTemperatureNotAvailable = 0xFFFFFFFF;

constexpr float criMockGpuTemperature = 45.5f;
constexpr uint32_t criMockMemoryTemperature = 40;
constexpr float criMockVrTemperatures[criVrTemperatureSensorCount] = {50.25f, 55.5f, 52.0f, 48.75f};
constexpr float criMockGpuBoardTemperatures[criGpuBoardTemperatureSensorCount] = {30.25f, 33.5f};
constexpr float criMockCompositeTemperature = 60.0f;

const std::map<std::string, std::pair<uint32_t, uint32_t>> criKeyOffsetMap = {
    {"AMB_TEMPERATURE", {criAmbientTemperatureOffset, criPunitTelemetryIndex}},
    {"COMPOSITE_TEMPERATURE", {criCompositeTemperatureOffset, criPunitTelemetryIndex}},
    {"VR_TEMPERATURE_0", {criVrTemperatureBaseOffset, criPunitTelemetryIndex}},
    {"VR_TEMPERATURE_1", {criVrTemperatureBaseOffset + 1, criPunitTelemetryIndex}},
    {"VR_TEMPERATURE_2", {criVrTemperatureBaseOffset + 2, criPunitTelemetryIndex}},
    {"VR_TEMPERATURE_3", {criVrTemperatureBaseOffset + 3, criPunitTelemetryIndex}},
    {"SOC_TOPDIE_TEMPERATURE", {criSocTopDieTemperatureOffset, criOobmsmTelemetryIndex}},
    {"VRAM_TEMPERATURE", {criVramTemperatureOffset, criOobmsmTelemetryIndex}}};

using CriTelemetryKey = std::pair<unsigned long, unsigned long>;
static std::map<CriTelemetryKey, uint64_t> criMockTelemetry;

static HANDLE mockCriPmtCreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
}

static BOOL mockCriPmtDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    auto readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
    auto telemetryEntry = criMockTelemetry.find(CriTelemetryKey(readRequest->index, readRequest->offset));
    if (telemetryEntry == criMockTelemetry.end()) {
        return FALSE;
    }
    if (dwIoControlCode == PmtSysman::ioctlPmtGetTelemetryQword) {
        *static_cast<uint64_t *>(lpOutBuffer) = telemetryEntry->second;
        *lpBytesReturned = sizeof(uint64_t);
    } else {
        *static_cast<uint32_t *>(lpOutBuffer) = static_cast<uint32_t>(telemetryEntry->second);
        *lpBytesReturned = sizeof(uint32_t);
    }
    return TRUE;
}

class SysmanProductHelperCriTemperatureTest : public SysmanProductHelperTemperatureTest {
  protected:
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> createFileBackup{&NEO::SysCalls::sysCallsCreateFile, mockCriPmtCreateFile};
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> deviceIoControlBackup{&NEO::SysCalls::sysCallsDeviceIoControl, mockCriPmtDeviceIoControl};

    void SetUp() override {
        SysmanProductHelperTemperatureTest::SetUp();
        pPmt->keyOffsetMap = criKeyOffsetMap;

        criMockTelemetry.clear();
        setPunitValue(criAmbientTemperatureOffset, (static_cast<uint64_t>(std::bit_cast<uint32_t>(criMockGpuBoardTemperatures[1])) << 32) | std::bit_cast<uint32_t>(criMockGpuBoardTemperatures[0]));
        setPunitValue(criCompositeTemperatureOffset, std::bit_cast<uint32_t>(criMockCompositeTemperature));
        for (uint32_t sensorIndex = 0; sensorIndex < criVrTemperatureSensorCount; sensorIndex++) {
            setPunitValue(criVrTemperatureBaseOffset + sensorIndex, std::bit_cast<uint32_t>(criMockVrTemperatures[sensorIndex]));
        }
        criMockTelemetry[CriTelemetryKey(criOobmsmTelemetryIndex, criSocTopDieTemperatureOffset)] = std::bit_cast<uint32_t>(criMockGpuTemperature);
        criMockTelemetry[CriTelemetryKey(criOobmsmTelemetryIndex, criVramTemperatureOffset)] = 0xAB00u | criMockMemoryTemperature;
    }

    void TearDown() override {
        criMockTelemetry.clear();
        SysmanProductHelperTemperatureTest::TearDown();
    }

    void setPunitValue(uint32_t offset, uint64_t value) {
        criMockTelemetry[CriTelemetryKey(criPunitTelemetryIndex, offset)] = value;
    }

    void removePunitValue(uint32_t offset) {
        criMockTelemetry.erase(CriTelemetryKey(criPunitTelemetryIndex, offset));
    }

    void removeOobmsmValue(uint32_t offset) {
        criMockTelemetry.erase(CriTelemetryKey(criOobmsmTelemetryIndex, offset));
    }

    zes_temp_handle_t getTempHandle(zes_temp_sensors_t type, uint32_t sensorIndex) {
        uint32_t occurrence = 0;
        for (auto handle : getTempHandles(criTemperatureHandleCount)) {
            zes_temp_properties_t properties = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
            if (properties.type == type && occurrence++ == sensorIndex) {
                return handle;
            }
        }
        return nullptr;
    }

    ze_result_t getTemperature(zes_temp_sensors_t type, uint32_t sensorIndex, double *pTemperature) {
        auto handle = getTempHandle(type, sensorIndex);
        EXPECT_NE(nullptr, handle);
        return zesTemperatureGetState(handle, pTemperature);
    }
};

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenPmtIsAvailableWhenEnumeratingTemperatureSensorsThenAllPmtBackedSensorsAreReturned, IsCRI) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(criTemperatureHandleCount, count);

    std::map<zes_temp_sensors_t, uint32_t> sensorTypeCount;
    for (auto handle : getTempHandles(count)) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        sensorTypeCount[properties.type]++;
    }
    EXPECT_EQ(1u, sensorTypeCount[ZES_TEMP_SENSORS_GLOBAL]);
    EXPECT_EQ(1u, sensorTypeCount[ZES_TEMP_SENSORS_GPU]);
    EXPECT_EQ(1u, sensorTypeCount[ZES_TEMP_SENSORS_MEMORY]);
    EXPECT_EQ(1u, sensorTypeCount[ZES_TEMP_SENSORS_VOLTAGE_REGULATOR]);
    EXPECT_EQ(1u, sensorTypeCount[ZES_TEMP_SENSORS_GPU_BOARD]);
    EXPECT_EQ(criVrTemperatureSensorCount, sensorTypeCount[ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE]);
    EXPECT_EQ(criGpuBoardTemperatureSensorCount, sensorTypeCount[ZES_TEMP_SENSORS_GPU_BOARD_SINGLE]);
    EXPECT_EQ(1u, sensorTypeCount[ZES_TEMP_SENSORS_COMPOSITE]);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenPmtIsNotAvailableWhenEnumeratingTemperatureSensorsThenNoHandlesAreReturned, IsCRI) {
    pWddmSysmanImp->pPmt.reset(nullptr);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenPmtBackedSensorWhenGettingPropertiesThenDefaultMaxTemperatureIsReturned, IsCRI) {
    const std::vector<zes_temp_sensors_t> pmtOnlySensorTypes = {ZES_TEMP_SENSORS_VOLTAGE_REGULATOR, ZES_TEMP_SENSORS_GPU_BOARD, ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE,
                                                                ZES_TEMP_SENSORS_GPU_BOARD_SINGLE, ZES_TEMP_SENSORS_COMPOSITE};
    for (auto sensorType : pmtOnlySensorTypes) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(getTempHandle(sensorType, 0), &properties));
        EXPECT_EQ(sensorType, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(0u, properties.subdeviceId);
        EXPECT_EQ(defaultMaxTemperature, properties.maxTemperature);
    }
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingGpuTemperatureThenSocTopDieTemperatureIsReturned, IsCRI) {
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_GPU, 0, &temperature));
    EXPECT_DOUBLE_EQ(criMockGpuTemperature, temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingMemoryTemperatureThenLowerByteOfVramTemperatureIsReturned, IsCRI) {
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_MEMORY, 0, &temperature));
    EXPECT_DOUBLE_EQ(static_cast<double>(criMockMemoryTemperature), temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingSingleVoltageRegulatorTemperaturesThenEachSensorValueIsReturned, IsCRI) {
    for (uint32_t sensorIndex = 0; sensorIndex < criVrTemperatureSensorCount; sensorIndex++) {
        double temperature = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE, sensorIndex, &temperature));
        EXPECT_DOUBLE_EQ(criMockVrTemperatures[sensorIndex], temperature);
    }
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingVoltageRegulatorTemperatureThenMaximumOfAllSensorsIsReturned, IsCRI) {
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR, 0, &temperature));
    EXPECT_DOUBLE_EQ(*std::max_element(std::begin(criMockVrTemperatures), std::end(criMockVrTemperatures)), temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenSomeVoltageRegulatorSensorsUnavailableWhenGettingVoltageRegulatorTemperatureThenMaximumOfAvailableSensorsIsReturned, IsCRI) {
    setPunitValue(criVrTemperatureBaseOffset, criTemperatureNotAvailable);
    removePunitValue(criVrTemperatureBaseOffset + 1);
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR, 0, &temperature));
    EXPECT_DOUBLE_EQ(std::max(criMockVrTemperatures[2], criMockVrTemperatures[3]), temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenAllVoltageRegulatorSensorsUnavailableWhenGettingVoltageRegulatorTemperatureThenNotAvailableIsReturned, IsCRI) {
    for (uint32_t sensorIndex = 0; sensorIndex < criVrTemperatureSensorCount; sensorIndex++) {
        setPunitValue(criVrTemperatureBaseOffset + sensorIndex, criTemperatureNotAvailable);
    }
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingSingleGpuBoardTemperaturesThenEachHalfOfAmbientTemperatureIsReturned, IsCRI) {
    for (uint32_t sensorIndex = 0; sensorIndex < criGpuBoardTemperatureSensorCount; sensorIndex++) {
        double temperature = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_GPU_BOARD_SINGLE, sensorIndex, &temperature));
        EXPECT_DOUBLE_EQ(criMockGpuBoardTemperatures[sensorIndex], temperature);
    }
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingGpuBoardTemperatureThenMaximumOfAllSensorsIsReturned, IsCRI) {
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_GPU_BOARD, 0, &temperature));
    EXPECT_DOUBLE_EQ(std::max(criMockGpuBoardTemperatures[0], criMockGpuBoardTemperatures[1]), temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenOneGpuBoardSensorUnavailableWhenGettingGpuBoardTemperatureThenOtherSensorValueIsReturned, IsCRI) {
    setPunitValue(criAmbientTemperatureOffset, (static_cast<uint64_t>(criTemperatureNotAvailable) << 32) | std::bit_cast<uint32_t>(criMockGpuBoardTemperatures[0]));
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getTemperature(ZES_TEMP_SENSORS_GPU_BOARD_SINGLE, 1, &temperature));
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_GPU_BOARD, 0, &temperature));
    EXPECT_DOUBLE_EQ(criMockGpuBoardTemperatures[0], temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenAmbientTemperatureReadFailsWhenGettingGpuBoardTemperatureThenErrorIsReturned, IsCRI) {
    removePunitValue(criAmbientTemperatureOffset);
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_GPU_BOARD_SINGLE, 0, &temperature));
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_GPU_BOARD, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingCompositeTemperatureThenCompositeTemperatureIsReturned, IsCRI) {
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_COMPOSITE, 0, &temperature));
    EXPECT_DOUBLE_EQ(criMockCompositeTemperature, temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenValidTelemetryWhenGettingGlobalTemperatureThenMaximumOfGpuMemoryVoltageRegulatorAndGpuBoardIsReturned, IsCRI) {
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_GLOBAL, 0, &temperature));
    const double expectedTemperature = std::max({static_cast<double>(criMockGpuTemperature), static_cast<double>(criMockMemoryTemperature),
                                                 static_cast<double>(*std::max_element(std::begin(criMockVrTemperatures), std::end(criMockVrTemperatures))),
                                                 static_cast<double>(std::max(criMockGpuBoardTemperatures[0], criMockGpuBoardTemperatures[1]))});
    EXPECT_DOUBLE_EQ(expectedTemperature, temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenTemperatureNotAvailableWhenGettingTemperatureStateThenNotAvailableIsReturned, IsCRI) {
    double temperature = 0;

    setPunitValue(criVrTemperatureBaseOffset + 2, criTemperatureNotAvailable);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE, 2, &temperature));

    setPunitValue(criCompositeTemperatureOffset, criTemperatureNotAvailable);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getTemperature(ZES_TEMP_SENSORS_COMPOSITE, 0, &temperature));

    criMockTelemetry[CriTelemetryKey(criOobmsmTelemetryIndex, criSocTopDieTemperatureOffset)] = criTemperatureNotAvailable;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getTemperature(ZES_TEMP_SENSORS_GPU, 0, &temperature));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, getTemperature(ZES_TEMP_SENSORS_GLOBAL, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenVoltageRegulatorSensorReportsInfinityOrNegativeValueWhenGettingTemperatureStateThenValueIsReturnedAsIs, IsCRI) {
    setPunitValue(criVrTemperatureBaseOffset + 1, std::bit_cast<uint32_t>(-5.0f));
    setPunitValue(criVrTemperatureBaseOffset + 2, std::bit_cast<uint32_t>(std::numeric_limits<float>::infinity()));
    double temperature = 0;

    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE, 1, &temperature));
    EXPECT_EQ(-5.0, temperature);

    EXPECT_EQ(ZE_RESULT_SUCCESS, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE, 2, &temperature));
    EXPECT_EQ(std::numeric_limits<double>::infinity(), temperature);
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenTelemetryReadFailsWhenGettingTemperatureStateThenErrorIsReturned, IsCRI) {
    double temperature = 0;

    removePunitValue(criVrTemperatureBaseOffset + 3);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_VOLTAGE_REGULATOR_SINGLE, 3, &temperature));

    removePunitValue(criCompositeTemperatureOffset);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_COMPOSITE, 0, &temperature));

    removeOobmsmValue(criVramTemperatureOffset);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_MEMORY, 0, &temperature));

    removeOobmsmValue(criSocTopDieTemperatureOffset);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_GPU, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenDeviceIsInLowPowerStateWhenGettingTemperatureStateThenDeviceInLowPowerStateErrorIsReturnedForAllSensors, IsCRI) {
    auto handles = getTempHandles(criTemperatureHandleCount);
    VariableBackup<DWORD> lastErrorBackup(&NEO::SysCalls::getLastErrorResults[0], ERROR_NOT_READY);
    criMockTelemetry.clear();

    for (auto handle : handles) {
        double temperature = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE, zesTemperatureGetState(handle, &temperature));
    }
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenMemoryTemperatureReadFailsWhenGettingGlobalTemperatureThenErrorIsReturned, IsCRI) {
    removeOobmsmValue(criVramTemperatureOffset);
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_GLOBAL, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenAllVoltageRegulatorSensorsUnavailableWhenGettingGlobalTemperatureThenErrorIsReturned, IsCRI) {
    for (uint32_t sensorIndex = 0; sensorIndex < criVrTemperatureSensorCount; sensorIndex++) {
        removePunitValue(criVrTemperatureBaseOffset + sensorIndex);
    }
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_GLOBAL, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenGpuBoardTemperatureReadFailsWhenGettingGlobalTemperatureThenErrorIsReturned, IsCRI) {
    removePunitValue(criAmbientTemperatureOffset);
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, getTemperature(ZES_TEMP_SENSORS_GLOBAL, 0, &temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenPmtIsNotAvailableWhenGettingSensorTemperatureThenUnsupportedFeatureIsReturned, IsCRI) {
    pWddmSysmanImp->pPmt.reset(nullptr);
    auto pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman, false, 0, 0);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GPU);
    double temperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&temperature));
}

HWTEST2_F(SysmanProductHelperCriTemperatureTest, GivenUnsupportedSensorTypeWhenQueryingTemperatureThenUnsupportedFeatureIsReturned, IsCRI) {
    auto pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman, false, 0, 0);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GLOBAL_MIN);
    EXPECT_FALSE(pWddmTemperatureImp->isTempModuleSupported());

    double temperature = 1;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&temperature));
    EXPECT_EQ(0, temperature);

    zes_temp_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getProperties(&properties));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
