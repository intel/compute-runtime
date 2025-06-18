/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ecc/sysman_ecc_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"

namespace L0 {
namespace Sysman {
zes_device_ecc_state_t EccImp::getEccState(uint8_t state) {
    switch (state) {
    case eccStateEnable:
        return ZES_DEVICE_ECC_STATE_ENABLED;
    case eccStateDisable:
        return ZES_DEVICE_ECC_STATE_DISABLED;
    default:
        return ZES_DEVICE_ECC_STATE_UNAVAILABLE;
    }
}

ze_result_t EccImp::getEccFwUtilInterface(FirmwareUtil *&pFwUtil) {
    pFwUtil = getFirmwareUtilInterface(pOsSysman);
    if (pFwUtil == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed while getting FirmwareUtilInterface() and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t EccImp::deviceEccAvailable(ze_bool_t *pAvailable) {
    if (pFwInterface == nullptr) {
        ze_result_t result = getEccFwUtilInterface(pFwInterface);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    return pFwInterface->fwGetEccAvailable(pAvailable);
}

ze_result_t EccImp::deviceEccConfigurable(ze_bool_t *pConfigurable) {
    if (pFwInterface == nullptr) {
        ze_result_t result = getEccFwUtilInterface(pFwInterface);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    return pFwInterface->fwGetEccConfigurable(pConfigurable);
}

ze_result_t EccImp::getEccState(zes_device_ecc_properties_t *pState) {
    if (pFwInterface == nullptr) {
        ze_result_t result = getEccFwUtilInterface(pFwInterface);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    uint8_t currentState = 0xff;
    uint8_t pendingState = 0xff;
    uint8_t defaultState = 0xff;
    ze_result_t result = pFwInterface->fwGetEccConfig(&currentState, &pendingState, &defaultState);

    pState->currentState = getEccState(currentState);
    pState->pendingState = getEccState(pendingState);

    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    void *pNext = pState->pNext;
    while (pNext) {
        zes_device_ecc_default_properties_ext_t *pExtProps = reinterpret_cast<zes_device_ecc_default_properties_ext_t *>(pNext);
        if (pExtProps->stype == ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT) {
            pExtProps->defaultState = getEccState(defaultState);
            break;
        }
        pNext = pExtProps->pNext;
    }

    pState->pendingAction = ZES_DEVICE_ACTION_WARM_CARD_RESET;
    if (pState->currentState == pState->pendingState) {
        pState->pendingAction = ZES_DEVICE_ACTION_NONE;
    }

    return result;
}

ze_result_t EccImp::setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) {
    if (pFwInterface == nullptr) {
        ze_result_t result = getEccFwUtilInterface(pFwInterface);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed while getting EccFwUtilInterface() and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    uint8_t state = 0;
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    if (newState->state == ZES_DEVICE_ECC_STATE_ENABLED) {
        state = eccStateEnable;
    } else if (newState->state == ZES_DEVICE_ECC_STATE_DISABLED) {
        state = eccStateDisable;
    } else if (newState->state == ZES_DEVICE_ECC_STATE_UNAVAILABLE) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid ecc enumeration and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_INVALID_ENUMERATION);
        return ZE_RESULT_ERROR_INVALID_ENUMERATION;
    }

    ze_result_t result = pFwInterface->fwSetEccConfig(state, &currentState, &pendingState);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set ecc configuration and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    pState->currentState = getEccState(currentState);
    pState->pendingState = getEccState(pendingState);

    pState->pendingAction = ZES_DEVICE_ACTION_WARM_CARD_RESET;
    if (pState->currentState == pState->pendingState) {
        pState->pendingAction = ZES_DEVICE_ACTION_NONE;
    }

    return result;
}

} // namespace Sysman
} // namespace L0
