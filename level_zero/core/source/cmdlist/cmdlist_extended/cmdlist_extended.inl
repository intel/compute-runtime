/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

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
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendSoftwareTag(const char *data) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
