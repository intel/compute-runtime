/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
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

ze_result_t ZE_APICALL zesIntelRasGetSupportedCategoriesExp(zes_ras_handle_t hRas, uint32_t *pCount, zes_ras_error_category_exp_t *pCategories) {
    if (L0::sysmanInitFromCore) {
        return L0::Ras::fromHandle(hRas)->rasGetSupportedCategoriesExp(pCount, pCategories);
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Ras::fromHandle(hRas)->rasGetSupportedCategoriesExp(pCount, pCategories);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelRasGetConfigExp(zes_ras_handle_t hRas, const uint32_t count, zes_intel_ras_config_exp_t *pConfig) {
    if (L0::sysmanInitFromCore) {
        return L0::Ras::fromHandle(hRas)->rasGetConfigExp(count, pConfig);
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Ras::fromHandle(hRas)->rasGetConfigExp(count, pConfig);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelRasSetConfigExp(zes_ras_handle_t hRas, const uint32_t count, const zes_intel_ras_config_exp_t *pConfig) {
    if (L0::sysmanInitFromCore) {
        return L0::Ras::fromHandle(hRas)->rasSetConfigExp(count, pConfig);
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Ras::fromHandle(hRas)->rasSetConfigExp(count, pConfig);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelRasGetStateExp(zes_ras_handle_t hRas, const uint32_t count, zes_intel_ras_state_exp_t *pState) {
    if (L0::sysmanInitFromCore) {
        return L0::Ras::fromHandle(hRas)->rasGetStateExp(count, pState);
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Ras::fromHandle(hRas)->rasGetStateExp(count, pState);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelPowerGetLimitsExp(zes_pwr_handle_t hPower, uint32_t *pLimit) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Power::fromHandle(hPower)->powerGetLimitsExp(pLimit);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelPowerSetLimitsExp(zes_pwr_handle_t hPower, const uint32_t limit) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Power::fromHandle(hPower)->powerSetLimitsExp(limit);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDeviceGetPowerUsageExp(zes_pwr_handle_t hPower, uint32_t *pInstantPower, uint32_t *pAveragePower) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::Power::fromHandle(hPower)->powerGetUsageExp(pInstantPower, pAveragePower);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    return L0::zesIntelDevicePciLinkSpeedUpdateExp(hDevice, downgradeUpgrade, pendingAction);
};

ze_result_t ZE_APICALL zesIntelRasGetSupportedCategoriesExp(zes_ras_handle_t hRas, uint32_t *pCount, zes_ras_error_category_exp_t *pCategories) {
    return L0::zesIntelRasGetSupportedCategoriesExp(hRas, pCount, pCategories);
}

ze_result_t ZE_APICALL zesIntelRasGetConfigExp(zes_ras_handle_t hRas, const uint32_t count, zes_intel_ras_config_exp_t *pConfig) {
    return L0::zesIntelRasGetConfigExp(hRas, count, pConfig);
}

ze_result_t ZE_APICALL zesIntelRasSetConfigExp(zes_ras_handle_t hRas, const uint32_t count, const zes_intel_ras_config_exp_t *pConfig) {
    return L0::zesIntelRasSetConfigExp(hRas, count, pConfig);
}

ze_result_t ZE_APICALL zesIntelRasGetStateExp(zes_ras_handle_t hRas, const uint32_t count, zes_intel_ras_state_exp_t *pState) {
    return L0::zesIntelRasGetStateExp(hRas, count, pState);
}

ze_result_t ZE_APICALL zesIntelPowerGetLimitsExp(zes_pwr_handle_t hPower, uint32_t *pLimit) {
    return L0::zesIntelPowerGetLimitsExp(hPower, pLimit);
}

ze_result_t ZE_APICALL zesIntelPowerSetLimitsExp(zes_pwr_handle_t hPower, const uint32_t limit) {
    return L0::zesIntelPowerSetLimitsExp(hPower, limit);
}

ze_result_t ZE_APICALL zesIntelDeviceGetPowerUsageExp(zes_pwr_handle_t hPower, uint32_t *pInstantPower, uint32_t *pAveragePower) {
    return L0::zesIntelDeviceGetPowerUsageExp(hPower, pInstantPower, pAveragePower);
}

} // extern "C"