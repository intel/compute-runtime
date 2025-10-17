/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session_sip_external_lib.h"

#include "shared/source/device/device.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

bool DebugSessionImp::openSipWrapper(NEO::Device *neoDevice, uint64_t contextHandle, uint64_t gpuVa) {
    return true;
}

bool DebugSessionImp::closeSipWrapper(NEO::Device *neoDevice, uint64_t contextHandle) {
    return true;
}

void DebugSessionImp::closeExternalSipHandles() {
}

SIP::regset_desc *DebugSessionImp::getRegsetDesc(zet_debug_regset_type_intel_gpu_t type, NEO::SipExternalLib *sipExternalLib) {
    return nullptr;
}

void *DebugSessionImp::getSipHandle(uint64_t contextHandle) {
    return nullptr;
}

bool DebugSessionImp::getRegisterAccessProperties(EuThread::ThreadId *threadId, uint32_t *pCount, zet_debug_regset_properties_t *pRegisterSetProperties) {
    return true;
}

uint32_t DebugSessionImp::getSipRegisterType(zet_debug_regset_type_intel_gpu_t zeRegisterType) {
    return static_cast<uint32_t>(zeRegisterType);
}

} // namespace L0