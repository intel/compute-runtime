/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ecc/ecc_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"

namespace L0 {
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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed while getting EccFwUtilInterface() and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        UNRECOVERABLE_IF(pFwInterface == nullptr);
    }

    *pAvailable = false;
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    ze_result_t result = pFwInterface->fwGetEccConfig(&currentState, &pendingState);
    if (ZE_RESULT_SUCCESS == result) {
        if ((currentState != eccStateNone) && (pendingState != eccStateNone)) {
            *pAvailable = true;
        }
    }

    return result;
}

ze_result_t EccImp::deviceEccConfigurable(ze_bool_t *pConfigurable) {
    return deviceEccAvailable(pConfigurable);
}

ze_result_t EccImp::getEccState(zes_device_ecc_properties_t *pState) {
    if (pFwInterface == nullptr) {
        ze_result_t result = getEccFwUtilInterface(pFwInterface);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed while getting EccFwUtilInterface() and returning error \n", __FUNCTION__, result);
            return result;
        }
        UNRECOVERABLE_IF(pFwInterface == nullptr);
    }

    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    ze_result_t result = pFwInterface->fwGetEccConfig(&currentState, &pendingState);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get ecc configuration and returning error:0x%x \n", __FUNCTION__, result);
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

ze_result_t EccImp::setEccState(const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) {
    if (pFwInterface == nullptr) {
        ze_result_t result = getEccFwUtilInterface(pFwInterface);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed while getting EccFwUtilInterface() and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        UNRECOVERABLE_IF(pFwInterface == nullptr);
    }

    uint8_t state = 0;
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    if (newState->state == ZES_DEVICE_ECC_STATE_ENABLED) {
        state = eccStateEnable;
    } else if (newState->state == ZES_DEVICE_ECC_STATE_DISABLED) {
        state = eccStateDisable;
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

} // namespace L0
