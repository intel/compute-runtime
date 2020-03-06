/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/zet_api.h>

#include "sysman/sysman.h"

extern "C" {

__zedllexport ze_result_t __zecall
zetSysmanGet(
    zet_device_handle_t hDevice,
    zet_sysman_version_t version,
    zet_sysman_handle_t *phSysman) {
    if (nullptr == hDevice) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == phSysman) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (ZET_SYSMAN_VERSION_CURRENT < version) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return L0::SysmanHandleContext::sysmanGet(hDevice, phSysman);
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceGetProperties(
    zet_sysman_handle_t hSysman,
    zet_sysman_properties_t *pProperties) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->deviceGetProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetCurrentMode(
    zet_sysman_handle_t hSysman,
    zet_sched_mode_t *pMode) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pMode)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerGetCurrentMode(pMode);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetTimeoutModeProperties(
    zet_sysman_handle_t hSysman,
    ze_bool_t getDefaults,
    zet_sched_timeout_properties_t *pConfig) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerGetTimeoutModeProperties(getDefaults, pConfig);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerGetTimesliceModeProperties(
    zet_sysman_handle_t hSysman,
    ze_bool_t getDefaults,
    zet_sched_timeslice_properties_t *pConfig) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerGetTimesliceModeProperties(getDefaults, pConfig);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetTimeoutMode(
    zet_sysman_handle_t hSysman,
    zet_sched_timeout_properties_t *pProperties,
    ze_bool_t *pNeedReboot) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pNeedReboot)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerSetTimeoutMode(pProperties, pNeedReboot);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetTimesliceMode(
    zet_sysman_handle_t hSysman,
    zet_sched_timeslice_properties_t *pProperties,
    ze_bool_t *pNeedReboot) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pNeedReboot)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerSetTimesliceMode(pProperties, pNeedReboot);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetExclusiveMode(
    zet_sysman_handle_t hSysman,
    ze_bool_t *pNeedReboot) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pNeedReboot)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerSetExclusiveMode(pNeedReboot);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanSchedulerSetComputeUnitDebugMode(
    zet_sysman_handle_t hSysman,
    ze_bool_t *pNeedReboot) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pNeedReboot)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->schedulerSetComputeUnitDebugMode(pNeedReboot);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanProcessesGetState(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_process_state_t *pProcesses) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->processesGetState(pCount, pProcesses);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceReset(
    zet_sysman_handle_t hSysman) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->deviceReset();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanDeviceGetRepairStatus(
    zet_sysman_handle_t hSysman,
    zet_repair_status_t *pRepairStatus) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pRepairStatus)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->deviceGetRepairStatus(pRepairStatus);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetProperties(
    zet_sysman_handle_t hSysman,
    zet_pci_properties_t *pProperties) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->pciGetProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetState(
    zet_sysman_handle_t hSysman,
    zet_pci_state_t *pState) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->pciGetState(pState);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetBars(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_pci_bar_properties_t *pProperties) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->pciGetBars(pCount, pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPciGetStats(
    zet_sysman_handle_t hSysman,
    zet_pci_stats_t *pStats) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pStats)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->pciGetStats(pStats);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_pwr_handle_t *phPower) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->powerGet(pCount, phPower);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetProperties(
    zet_sysman_pwr_handle_t hPower,
    zet_power_properties_t *pProperties) {
    try {
        {
            if (nullptr == hPower)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetEnergyCounter(
    zet_sysman_pwr_handle_t hPower,
    zet_power_energy_counter_t *pEnergy) {
    try {
        {
            if (nullptr == hPower)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pEnergy)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetLimits(
    zet_sysman_pwr_handle_t hPower,
    zet_power_sustained_limit_t *pSustained,
    zet_power_burst_limit_t *pBurst,
    zet_power_peak_limit_t *pPeak) {
    try {
        {
            if (nullptr == hPower)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerSetLimits(
    zet_sysman_pwr_handle_t hPower,
    const zet_power_sustained_limit_t *pSustained,
    const zet_power_burst_limit_t *pBurst,
    const zet_power_peak_limit_t *pPeak) {
    try {
        {
            if (nullptr == hPower)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerGetEnergyThreshold(
    zet_sysman_pwr_handle_t hPower,
    zet_energy_threshold_t *pThreshold) {
    try {
        {
            if (nullptr == hPower)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pThreshold)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPowerSetEnergyThreshold(
    zet_sysman_pwr_handle_t hPower,
    double threshold) {
    try {
        {
            if (nullptr == hPower)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_freq_handle_t *phFrequency) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->frequencyGet(pCount, phFrequency);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetProperties(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_properties_t *pProperties) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Frequency::fromHandle(hFrequency)->frequencyGetProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetAvailableClocks(
    zet_sysman_freq_handle_t hFrequency,
    uint32_t *pCount,
    double *phFrequency) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetRange(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_range_t *pLimits) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pLimits)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Frequency::fromHandle(hFrequency)->frequencyGetRange(pLimits);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencySetRange(
    zet_sysman_freq_handle_t hFrequency,
    const zet_freq_range_t *pLimits) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pLimits)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Frequency::fromHandle(hFrequency)->frequencySetRange(pLimits);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetState(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_state_t *pState) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Frequency::fromHandle(hFrequency)->frequencyGetState(pState);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyGetThrottleTime(
    zet_sysman_freq_handle_t hFrequency,
    zet_freq_throttle_time_t *pThrottleTime) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pThrottleTime)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetCapabilities(
    zet_sysman_freq_handle_t hFrequency,
    zet_oc_capabilities_t *pOcCapabilities) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pOcCapabilities)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetConfig(
    zet_sysman_freq_handle_t hFrequency,
    zet_oc_config_t *pOcConfiguration) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pOcConfiguration)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetConfig(
    zet_sysman_freq_handle_t hFrequency,
    zet_oc_config_t *pOcConfiguration,
    ze_bool_t *pDeviceRestart) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pOcConfiguration)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetIccMax(
    zet_sysman_freq_handle_t hFrequency,
    double *pOcIccMax) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pOcIccMax)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetIccMax(
    zet_sysman_freq_handle_t hFrequency,
    double ocIccMax) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcGetTjMax(
    zet_sysman_freq_handle_t hFrequency,
    double *pOcTjMax) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pOcTjMax)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFrequencyOcSetTjMax(
    zet_sysman_freq_handle_t hFrequency,
    double ocTjMax) {
    try {
        {
            if (nullptr == hFrequency)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_engine_handle_t *phEngine) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->engineGet(pCount, phEngine);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGetProperties(
    zet_sysman_engine_handle_t hEngine,
    zet_engine_properties_t *pProperties) {
    try {
        {
            if (nullptr == hEngine)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEngineGetActivity(
    zet_sysman_engine_handle_t hEngine,
    zet_engine_stats_t *pStats) {
    try {
        {
            if (nullptr == hEngine)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pStats)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_standby_handle_t *phStandby) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->standbyGet(pCount, phStandby);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGetProperties(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_properties_t *pProperties) {
    try {
        {
            if (nullptr == hStandby)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Standby::fromHandle(hStandby)->standbyGetProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanStandbyGetMode(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_promo_mode_t *pMode) {
    try {
        {
            if (nullptr == hStandby)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pMode)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Standby::fromHandle(hStandby)->standbyGetMode(pMode);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanStandbySetMode(
    zet_sysman_standby_handle_t hStandby,
    zet_standby_promo_mode_t mode) {
    try {
        {
            if (nullptr == hStandby)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZET_STANDBY_PROMO_MODE_DEFAULT != mode && ZET_STANDBY_PROMO_MODE_NEVER != mode)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Standby::fromHandle(hStandby)->standbySetMode(mode);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_firmware_handle_t *phFirmware) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->firmwareGet(pCount, phFirmware);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGetProperties(
    zet_sysman_firmware_handle_t hFirmware,
    zet_firmware_properties_t *pProperties) {
    try {
        {
            if (nullptr == hFirmware)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareGetChecksum(
    zet_sysman_firmware_handle_t hFirmware,
    uint32_t *pChecksum) {
    try {
        {
            if (nullptr == hFirmware)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pChecksum)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFirmwareFlash(
    zet_sysman_firmware_handle_t hFirmware,
    void *pImage,
    uint32_t size) {
    try {
        {
            if (nullptr == hFirmware)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pImage)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_mem_handle_t *phMemory) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->memoryGet(pCount, phMemory);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetProperties(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_properties_t *pProperties) {
    try {
        {
            if (nullptr == hMemory)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetState(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_state_t *pState) {
    try {
        {
            if (nullptr == hMemory)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanMemoryGetBandwidth(
    zet_sysman_mem_handle_t hMemory,
    zet_mem_bandwidth_t *pBandwidth) {
    try {
        {
            if (nullptr == hMemory)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pBandwidth)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_fabric_port_handle_t *phPort) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->fabricPortGet(pCount, phPort);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetProperties(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_properties_t *pProperties) {
    try {
        {
            if (nullptr == hPort)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetLinkType(
    zet_sysman_fabric_port_handle_t hPort,
    ze_bool_t verbose,
    zet_fabric_link_type_t *pLinkType) {
    try {
        {
            if (nullptr == hPort)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pLinkType)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetConfig(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_config_t *pConfig) {
    try {
        {
            if (nullptr == hPort)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortSetConfig(
    zet_sysman_fabric_port_handle_t hPort,
    const zet_fabric_port_config_t *pConfig) {
    try {
        {
            if (nullptr == hPort)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetState(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_state_t *pState) {
    try {
        {
            if (nullptr == hPort)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFabricPortGetThroughput(
    zet_sysman_fabric_port_handle_t hPort,
    zet_fabric_port_throughput_t *pThroughput) {
    try {
        {
            if (nullptr == hPort)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pThroughput)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_temp_handle_t *phTemperature) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->temperatureGet(pCount, phTemperature);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetProperties(
    zet_sysman_temp_handle_t hTemperature,
    zet_temp_properties_t *pProperties) {
    try {
        {
            if (nullptr == hTemperature)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetConfig(
    zet_sysman_temp_handle_t hTemperature,
    zet_temp_config_t *pConfig) {
    try {
        {
            if (nullptr == hTemperature)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureSetConfig(
    zet_sysman_temp_handle_t hTemperature,
    const zet_temp_config_t *pConfig) {
    try {
        {
            if (nullptr == hTemperature)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanTemperatureGetState(
    zet_sysman_temp_handle_t hTemperature,
    double *pTemperature) {
    try {
        {
            if (nullptr == hTemperature)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pTemperature)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_psu_handle_t *phPsu) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->psuGet(pCount, phPsu);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGetProperties(
    zet_sysman_psu_handle_t hPsu,
    zet_psu_properties_t *pProperties) {
    try {
        {
            if (nullptr == hPsu)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanPsuGetState(
    zet_sysman_psu_handle_t hPsu,
    zet_psu_state_t *pState) {
    try {
        {
            if (nullptr == hPsu)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFanGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_fan_handle_t *phFan) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->fanGet(pCount, phFan);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetProperties(
    zet_sysman_fan_handle_t hFan,
    zet_fan_properties_t *pProperties) {
    try {
        {
            if (nullptr == hFan)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetConfig(
    zet_sysman_fan_handle_t hFan,
    zet_fan_config_t *pConfig) {
    try {
        {
            if (nullptr == hFan)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFanSetConfig(
    zet_sysman_fan_handle_t hFan,
    const zet_fan_config_t *pConfig) {
    try {
        {
            if (nullptr == hFan)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanFanGetState(
    zet_sysman_fan_handle_t hFan,
    zet_fan_speed_units_t units,
    uint32_t *pSpeed) {
    try {
        {
            if (nullptr == hFan)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pSpeed)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanLedGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_led_handle_t *phLed) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->ledGet(pCount, phLed);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanLedGetProperties(
    zet_sysman_led_handle_t hLed,
    zet_led_properties_t *pProperties) {
    try {
        {
            if (nullptr == hLed)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanLedGetState(
    zet_sysman_led_handle_t hLed,
    zet_led_state_t *pState) {
    try {
        {
            if (nullptr == hLed)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanLedSetState(
    zet_sysman_led_handle_t hLed,
    const zet_led_state_t *pState) {
    try {
        {
            if (nullptr == hLed)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pState)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanRasGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_ras_handle_t *phRas) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->rasGet(pCount, phRas);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetProperties(
    zet_sysman_ras_handle_t hRas,
    zet_ras_properties_t *pProperties) {
    try {
        {
            if (nullptr == hRas)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetConfig(
    zet_sysman_ras_handle_t hRas,
    zet_ras_config_t *pConfig) {
    try {
        {
            if (nullptr == hRas)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanRasSetConfig(
    zet_sysman_ras_handle_t hRas,
    const zet_ras_config_t *pConfig) {
    try {
        {
            if (nullptr == hRas)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanRasGetState(
    zet_sysman_ras_handle_t hRas,
    ze_bool_t clear,
    uint64_t *pTotalErrors,
    zet_ras_details_t *pDetails) {
    try {
        {
            if (nullptr == hRas)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pTotalErrors)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEventGet(
    zet_sysman_handle_t hSysman,
    zet_sysman_event_handle_t *phEvent) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->eventGet(phEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEventGetConfig(
    zet_sysman_event_handle_t hEvent,
    zet_event_config_t *pConfig) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEventSetConfig(
    zet_sysman_event_handle_t hEvent,
    const zet_event_config_t *pConfig) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pConfig)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEventGetState(
    zet_sysman_event_handle_t hEvent,
    ze_bool_t clear,
    uint32_t *pEvents) {
    try {
        {
            if (nullptr == hEvent)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pEvents)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanEventListen(
    ze_driver_handle_t hDriver,
    uint32_t timeout,
    uint32_t count,
    zet_sysman_event_handle_t *phEvents,
    uint32_t *pEvents) {
    try {
        {
            if (nullptr == hDriver)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEvents)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phEvents)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_ERROR_UNKNOWN;
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGet(
    zet_sysman_handle_t hSysman,
    uint32_t *pCount,
    zet_sysman_diag_handle_t *phDiagnostics) {
    try {
        {
            if (nullptr == hSysman)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Sysman::fromHandle(hSysman)->diagnosticsGet(pCount, phDiagnostics);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGetProperties(
    zet_sysman_diag_handle_t hDiagnostics,
    zet_diag_properties_t *pProperties) {
    try {
        {
            if (nullptr == hDiagnostics)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsGetTests(
    zet_sysman_diag_handle_t hDiagnostics,
    uint32_t *pCount,
    zet_diag_test_t *pTests) {
    try {
        {
            if (nullptr == hDiagnostics)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetSysmanDiagnosticsRunTests(
    zet_sysman_diag_handle_t hDiagnostics,
    uint32_t start,
    uint32_t end,
    zet_diag_result_t *pResult) {
    try {
        {
            if (nullptr == hDiagnostics)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pResult)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
