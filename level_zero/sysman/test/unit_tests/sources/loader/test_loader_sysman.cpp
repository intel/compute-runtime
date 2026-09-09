/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ddi_table_expectations.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace ult {

// Single manifest of all L0 sysman DDI tables. Every entry declared by the L0 headers is listed
// here, in header declaration order, together with the API version that first exposes it.
//
// This is the only place that has to be updated when a zes*GetProcAddrTable() implementation
// gains, drops or re-versions an entry, or when the L0 headers add a new DDI slot. Any such change
// that is not reflected here makes the tests below fail.

namespace ZesDdiTableManifest {

namespace zesDevice {
using Table = zes_device_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReset, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnProcessesGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnPciGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnPciGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnPciGetBars, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnPciGetStats, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumDiagnosticTestSuites, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumEngineGroups, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEventRegister, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumFabricPorts, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumFans, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumFirmwares, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumFrequencyDomains, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumLeds, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumMemoryModules, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumPerformanceFactorDomains, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumPowerDomains, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetCardPowerDomain, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnEnumPsus, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumRasErrorSets, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumSchedulers, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumStandbyDomains, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEnumTemperatureSensors, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEccAvailable, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnEccConfigurable, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGetEccState, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnSetEccState, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGet, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnSetOverclockWaiver, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetOverclockDomains, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetOverclockControls, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnResetOverclockSettings, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnReadOverclockState, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnEnumOverclockDomains, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnResetExt, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnPciLinkSpeedUpdateExt, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnGetHealthStatusExt, ZE_API_VERSION_1_18),
    DDI_ENTRY(pfnSetHealthStatusExt, ZE_API_VERSION_1_18),
};
} // namespace zesDevice

namespace zesGlobal {
using Table = zes_global_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnInit, ZE_API_VERSION_1_5),
};
} // namespace zesGlobal

namespace zesDriver {
using Table = zes_driver_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnEventListen, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEventListenEx, ZE_API_VERSION_1_1),
    DDI_ENTRY(pfnGet, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetExtensionProperties, ZE_API_VERSION_1_8),
    DDI_ENTRY(pfnGetExtensionFunctionAddress, ZE_API_VERSION_1_8),
};
} // namespace zesDriver

namespace zesDiagnostics {
using Table = zes_diagnostics_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetTests, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnRunTests, ZE_API_VERSION_1_0),
};
} // namespace zesDiagnostics

namespace zesEngine {
using Table = zes_engine_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetActivity, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnGetActivityExt, ZE_API_VERSION_1_7),
};
} // namespace zesEngine

namespace zesFabricPort {
using Table = zes_fabric_port_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetLinkType, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetThroughput, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetFabricErrorCounters, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnGetMultiPortThroughput, ZE_API_VERSION_1_7),
};
} // namespace zesFabricPort

namespace zesFan {
using Table = zes_fan_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetDefaultMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetFixedSpeedMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetSpeedTableMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
};
} // namespace zesFan

namespace zesFirmware {
using Table = zes_firmware_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnFlash, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetFlashProgress, ZE_API_VERSION_1_8),
    DDI_ENTRY(pfnGetConsoleLogs, ZE_API_VERSION_1_9),
};
} // namespace zesFirmware

namespace zesFrequency {
using Table = zes_frequency_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetAvailableClocks, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetRange, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetRange, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetThrottleTime, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcGetCapabilities, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcGetFrequencyTarget, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcSetFrequencyTarget, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcGetVoltageTarget, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcSetVoltageTarget, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcSetMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcGetMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcGetIccMax, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcSetIccMax, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcGetTjMax, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOcSetTjMax, ZE_API_VERSION_1_0),
};
} // namespace zesFrequency

namespace zesLed {
using Table = zes_led_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetColor, ZE_API_VERSION_1_0),
};
} // namespace zesLed

namespace zesMemory {
using Table = zes_memory_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetBandwidth, ZE_API_VERSION_1_0),
};
} // namespace zesMemory

namespace zesPerformanceFactor {
using Table = zes_performance_factor_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetConfig, ZE_API_VERSION_1_0),
};
} // namespace zesPerformanceFactor

namespace zesPower {
using Table = zes_power_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetEnergyCounter, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetLimits, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetLimits, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetEnergyThreshold, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetEnergyThreshold, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetLimitsExt, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetLimitsExt, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetUsage, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnGetLimitsExt2, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnSetLimitsExt2, ZE_API_VERSION_1_16),
};
} // namespace zesPower

namespace zesPsu {
using Table = zes_psu_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
};
} // namespace zesPsu

namespace zesRas {
using Table = zes_ras_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
};
} // namespace zesRas

namespace zesRasExp {
using Table = zes_ras_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetStateExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnClearStateExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnGetSupportedCategoriesExp, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnGetStateExp2, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnGetConfigExp, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnSetConfigExp, ZE_API_VERSION_1_16),
};
} // namespace zesRasExp

namespace zesDriverExp {
using Table = zes_driver_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetDeviceByUuidExp, ZE_API_VERSION_1_9),
};
} // namespace zesDriverExp

namespace zesDeviceExp {
using Table = zes_device_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetSubDevicePropertiesExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnEnumActiveVFExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnEnumEnabledVFExp, ZE_API_VERSION_1_10),
};
} // namespace zesDeviceExp

namespace zesVFManagementExp {
using Table = zes_vf_management_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetVFPropertiesExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetVFMemoryUtilizationExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetVFEngineUtilizationExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnSetVFTelemetryModeExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnSetVFTelemetrySamplingIntervalExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetVFCapabilitiesExp, ZE_API_VERSION_1_10),
    DDI_ENTRY(pfnGetVFMemoryUtilizationExp2, ZE_API_VERSION_1_10),
    DDI_ENTRY(pfnGetVFEngineUtilizationExp2, ZE_API_VERSION_1_10),
    DDI_ENTRY(pfnGetVFCapabilitiesExp2, ZE_API_VERSION_1_12),
};
} // namespace zesVFManagementExp

namespace zesFirmwareExp {
using Table = zes_firmware_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetSecurityVersionExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnSetSecurityVersionExp, ZE_API_VERSION_1_9),
};
} // namespace zesFirmwareExp

namespace zesScheduler {
using Table = zes_scheduler_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetCurrentMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetTimeoutModeProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetTimesliceModeProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetTimeoutMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetTimesliceMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetExclusiveMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetComputeUnitDebugMode, ZE_API_VERSION_1_0),
};
} // namespace zesScheduler

namespace zesStandby {
using Table = zes_standby_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetMode, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetMode, ZE_API_VERSION_1_0),
};
} // namespace zesStandby

namespace zesTemperature {
using Table = zes_temperature_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetState, ZE_API_VERSION_1_0),
};
} // namespace zesTemperature

namespace zesOverclock {
using Table = zes_overclock_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetDomainProperties, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetDomainVFProperties, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetDomainControlProperties, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetControlCurrentValue, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetControlPendingValue, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnSetControlUserValue, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetControlState, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetVFPointValues, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnSetVFPointValues, ZE_API_VERSION_1_5),
};
} // namespace zesOverclock

constexpr DdiTableExpectation ddiTables[] = {
    DDI_TABLE(zesDevice, zesGetDeviceProcAddrTable),
    DDI_TABLE(zesGlobal, zesGetGlobalProcAddrTable),
    DDI_TABLE(zesDriver, zesGetDriverProcAddrTable),
    DDI_TABLE(zesDiagnostics, zesGetDiagnosticsProcAddrTable),
    DDI_TABLE(zesEngine, zesGetEngineProcAddrTable),
    DDI_TABLE(zesFabricPort, zesGetFabricPortProcAddrTable),
    DDI_TABLE(zesFan, zesGetFanProcAddrTable),
    DDI_TABLE(zesFirmware, zesGetFirmwareProcAddrTable),
    DDI_TABLE(zesFrequency, zesGetFrequencyProcAddrTable),
    DDI_TABLE(zesLed, zesGetLedProcAddrTable),
    DDI_TABLE(zesMemory, zesGetMemoryProcAddrTable),
    DDI_TABLE(zesPerformanceFactor, zesGetPerformanceFactorProcAddrTable),
    DDI_TABLE(zesPower, zesGetPowerProcAddrTable),
    DDI_TABLE(zesPsu, zesGetPsuProcAddrTable),
    DDI_TABLE(zesRas, zesGetRasProcAddrTable),
    DDI_TABLE(zesRasExp, zesGetRasExpProcAddrTable),
    DDI_TABLE(zesDriverExp, zesGetDriverExpProcAddrTable),
    DDI_TABLE(zesDeviceExp, zesGetDeviceExpProcAddrTable),
    DDI_TABLE(zesVFManagementExp, zesGetVFManagementExpProcAddrTable),
    DDI_TABLE(zesFirmwareExp, zesGetFirmwareExpProcAddrTable),
    DDI_TABLE(zesScheduler, zesGetSchedulerProcAddrTable),
    DDI_TABLE(zesStandby, zesGetStandbyProcAddrTable),
    DDI_TABLE(zesTemperature, zesGetTemperatureProcAddrTable),
    DDI_TABLE(zesOverclock, zesGetOverclockProcAddrTable),
};

} // namespace ZesDdiTableManifest

constexpr size_t ddiTablesCount = std::size(ZesDdiTableManifest::ddiTables);
constexpr const DdiTableExpectation *ddiTables = ZesDdiTableManifest::ddiTables;

TEST(ZesDdiTableManifestTest, givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnce) {
    givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnceFunction(ddiTables, ddiTablesCount);
}

TEST(ZesGetProcAddrTableTest, givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulated) {
    givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZesGetProcAddrTableTest, givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulated) {
    givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZesGetProcAddrTableTest, givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturned) {
    givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturnedFunction(ddiTables, ddiTablesCount);
}

} // namespace ult
} // namespace L0
