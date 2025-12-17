/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session_sip_external_lib.h"

#include "shared/source/device/device.h"
#include "shared/source/sip_external_lib/sip_external_lib.h"

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

bool DebugSessionImp::getRegHeaderSize(const NEO::StateSaveAreaHeader *pStateSaveArea, size_t size, size_t &regHeaderSize) {
    if (pStateSaveArea->versionHeader.version.major == 3) {
        DEBUG_BREAK_IF(size != sizeof(NEO::StateSaveAreaHeader::regHeaderV3) + sizeof(NEO::StateSaveAreaHeader::versionHeader));
        regHeaderSize = sizeof(SIP::intelgt_state_save_area_V3);
        return true;
    } else if (pStateSaveArea->versionHeader.version.major < 3) {
        DEBUG_BREAK_IF(size != sizeof(NEO::StateSaveAreaHeader::regHeader) + sizeof(NEO::StateSaveAreaHeader::versionHeader));
        regHeaderSize = sizeof(SIP::intelgt_state_save_area);
        return true;
    }

    return false;
}

size_t DebugSessionImp::getSipCommandRegisterSize(bool write, size_t size) {
    return size;
}

size_t DebugSessionImp::getSipCommandRegisterValues(NEO::SipCommandRegisterValues &command, bool write, size_t size) {
    return getSipCommandRegisterSize(write, size);
}

bool DebugSessionImp::getThreadSipCounter(const void *stateSaveArea, L0::EuThread *thread, const NEO::StateSaveAreaHeader *stateSaveAreaHeader, uint64_t *sipThreadCounter) {
    return false;
}

void DebugSessionImp::getFifoOffsets(const NEO::StateSaveAreaHeader *stateSaveAreaHeader, uint64_t &offsetTail, uint64_t &offsetFifoSize, uint64_t &offsetFifo, uint64_t gpuVa) {
    offsetTail = (sizeof(SIP::StateSaveArea)) + offsetof(struct SIP::intelgt_state_save_area_V3, fifo_tail);
    offsetFifoSize = (sizeof(SIP::StateSaveArea)) + offsetof(struct SIP::intelgt_state_save_area_V3, fifo_size);
    offsetFifo = gpuVa + (stateSaveAreaHeader->versionHeader.size * 8) + stateSaveAreaHeader->regHeaderV3.fifo_offset;
}

} // namespace L0