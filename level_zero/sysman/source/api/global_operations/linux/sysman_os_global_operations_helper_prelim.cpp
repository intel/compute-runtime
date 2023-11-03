/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/global_operations/linux/sysman_os_global_operations_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"

namespace L0 {
namespace Sysman {
void LinuxGlobalOperationsImp::getRepairStatus(zes_device_state_t *pState) {
    bool ifrStatus = false;
    if (IGFX_PVC == pLinuxSysmanImp->getParentSysmanDeviceImp()->getProductFamily()) {
        auto pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
        if (pFwInterface != nullptr) {
            auto result = pFwInterface->fwIfrApplied(ifrStatus);
            if (result == ZE_RESULT_SUCCESS) {
                pState->repaired = ZES_REPAIR_STATUS_NOT_PERFORMED;
                if (ifrStatus) {
                    pState->reset |= ZES_RESET_REASON_FLAG_REPAIR;
                    pState->repaired = ZES_REPAIR_STATUS_PERFORMED;
                }
            }
        }
    }
}
} // namespace Sysman
} // namespace L0
