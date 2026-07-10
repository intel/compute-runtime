/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::pciLinkSpeedUpdate(hDevice, downgradeUpgrade, pendingAction);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDeviceMemoryGetPageOfflineStateExp(zes_device_handle_t hDevice, zes_intel_mem_page_status_exp_t pageStatus, uint32_t *pCount, zes_intel_mem_page_info_exp_t *pPageOfflineInfo) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::memoryGetPageOfflineStateExp(hDevice, pageStatus, pCount, pPageOfflineInfo);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDeviceGetHealthExp(zes_device_handle_t hDevice, zes_intel_device_health_status_exp_t *pHealth) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::getDeviceHealthExp(hDevice, pHealth);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDeviceSetHealthExp(zes_device_handle_t hDevice, zes_intel_device_health_status_exp_t health, const char *pReason, const uint32_t authTokenLength, const char *pAuthToken) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::setDeviceHealthExp(hDevice, health, pReason, authTokenLength, pAuthToken);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDriverEnumInfoLogsExp(zes_driver_handle_t hDriver, uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDriverHandle::fromHandle(hDriver)->enumInfoLogs(pCount, phInfoLogs);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogGetPropertiesExp(zes_intel_info_log_handle_t hInfoLog, zes_intel_info_log_properties_exp_t *pInfoLogProperties) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLog::fromHandle(hInfoLog)->infoLogGetProperties(pInfoLogProperties);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogReadExp(zes_intel_info_log_handle_t hInfoLog, uint32_t *pSize, uint8_t *pBuffer) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLog::fromHandle(hInfoLog)->infoLogRead(pSize, pBuffer);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogEnableExp(zes_intel_info_log_handle_t hInfoLog, bool state) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLog::fromHandle(hInfoLog)->infoLogEnable(state);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    return L0::zesIntelDevicePciLinkSpeedUpdateExp(hDevice, downgradeUpgrade, pendingAction);
};

ze_result_t ZE_APICALL zesIntelDeviceMemoryGetPageOfflineStateExp(zes_device_handle_t hDevice, zes_intel_mem_page_status_exp_t pageStatus, uint32_t *pCount, zes_intel_mem_page_info_exp_t *pPageOfflineInfo) {
    return L0::zesIntelDeviceMemoryGetPageOfflineStateExp(hDevice, pageStatus, pCount, pPageOfflineInfo);
}

ze_result_t ZE_APICALL zesIntelDeviceGetHealthExp(zes_device_handle_t hDevice, zes_intel_device_health_status_exp_t *pHealth) {
    return L0::zesIntelDeviceGetHealthExp(hDevice, pHealth);
}

ze_result_t ZE_APICALL zesIntelDeviceSetHealthExp(zes_device_handle_t hDevice, zes_intel_device_health_status_exp_t health, const char *pReason, const uint32_t authTokenLength, const char *pAuthToken) {
    return L0::zesIntelDeviceSetHealthExp(hDevice, health, pReason, authTokenLength, pAuthToken);
}

ze_result_t ZE_APICALL zesIntelDriverEnumInfoLogsExp(zes_driver_handle_t hDriver, uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {
    return L0::zesIntelDriverEnumInfoLogsExp(hDriver, pCount, phInfoLogs);
}

ze_result_t ZE_APICALL zesIntelInfoLogGetPropertiesExp(zes_intel_info_log_handle_t hInfoLog, zes_intel_info_log_properties_exp_t *pInfoLogProperties) {
    return L0::zesIntelInfoLogGetPropertiesExp(hInfoLog, pInfoLogProperties);
}

ze_result_t ZE_APICALL zesIntelInfoLogReadExp(zes_intel_info_log_handle_t hInfoLog, uint32_t *pSize, uint8_t *pBuffer) {
    return L0::zesIntelInfoLogReadExp(hInfoLog, pSize, pBuffer);
}

ze_result_t ZE_APICALL zesIntelInfoLogEnableExp(zes_intel_info_log_handle_t hInfoLog, bool state) {
    return L0::zesIntelInfoLogEnableExp(hInfoLog, state);
}

} // extern "C"
