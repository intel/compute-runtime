/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMILoadRegImm(uint32_t reg, uint32_t value) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMILoadRegReg(uint32_t reg1, uint32_t reg2) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMILoadRegMem(uint32_t reg1, uint64_t address) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMIStoreRegMem(uint32_t reg1, uint64_t address) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMIMath(void *aluArray, size_t aluCount) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMIBBStart(uint64_t address,
                                                                  size_t predication,
                                                                  bool secondLevel) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMIBBEnd() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMINoop() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendPipeControl(void *dstPtr, uint64_t value) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWaitOnMemory(void *desc,
                                                                     void *ptr,
                                                                     uint32_t data,
                                                                     ze_event_handle_t hSignalEvent) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWriteToMemory(void *desc,
                                                                      void *ptr,
                                                                      uint64_t data) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
