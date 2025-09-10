/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/linux/xe/debug_session_sip_external_lib.h"

namespace L0 {

bool DebugSessionLinuxXe::openSipWrapper(NEO::Device *neoDevice, uint64_t contextHandle, uint64_t gpuVa) {
    return true;
}

bool DebugSessionLinuxXe::closeSipWrapper(NEO::Device *neoDevice, uint64_t contextHandle) {
    return true;
}

void DebugSessionLinuxXe::closeExternalSipHandles() {
}

} // namespace L0