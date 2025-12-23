/*
 * Copyright (C) 2025 Intel Corporation
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

uint32_t DebugSessionImp::getSipRegisterType(zet_debug_regset_type_intel_gpu_t zeRegisterType) {
    return static_cast<uint32_t>(zeRegisterType);
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

ze_result_t DebugSessionImp::getCommandRegisterDescriptor(const NEO::StateSaveAreaHeader *stateSaveAreaHeader, SIP::regset_desc *regdesc) {
    if (stateSaveAreaHeader->versionHeader.version.major == 3) {
        *regdesc = std::move(stateSaveAreaHeader->regHeaderV3.cmd);
    } else if (stateSaveAreaHeader->versionHeader.version.major < 3) {
        *regdesc = std::move(stateSaveAreaHeader->regHeader.cmd);
    } else {
        PRINT_DEBUGGER_ERROR_LOG("%s: Unsupported version of State Save Area Header\n", __func__);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

size_t DebugSessionImp::getSipCommandRegisterValues(NEO::SipCommandRegisterValues &command, bool write, size_t size) {
    return size;
}

void DebugSessionImp::getFifoOffsets(const NEO::StateSaveAreaHeader *stateSaveAreaHeader, uint64_t &offsetTail, uint64_t &offsetFifoSize, uint64_t &offsetFifo, uint64_t gpuVa) {
    offsetTail = (sizeof(SIP::StateSaveArea)) + offsetof(struct SIP::intelgt_state_save_area_V3, fifo_tail);
    offsetFifoSize = (sizeof(SIP::StateSaveArea)) + offsetof(struct SIP::intelgt_state_save_area_V3, fifo_size);
    offsetFifo = gpuVa + (stateSaveAreaHeader->versionHeader.size * 8) + stateSaveAreaHeader->regHeaderV3.fifo_offset;
}

} // namespace L0