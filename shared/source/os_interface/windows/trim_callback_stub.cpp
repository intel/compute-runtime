/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

namespace NEO {

VOID *Wddm::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) {
    return nullptr;
}

void Wddm::unregisterTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, VOID *trimCallbackHandle) {
}

void APIENTRY WddmResidencyController::trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification) {
}

void WddmResidencyController::trimResidency(const D3DDDI_TRIMRESIDENCYSET_FLAGS &flags, uint64_t bytes) {
}

bool WddmResidencyController::trimResidencyToBudget(uint64_t bytes) {
    return false;
}

} // namespace NEO