/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"

namespace NEO {

template <>
void HardwareParse::findCsrBaseAddress<GenGfxFamily>() {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename GenGfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<STATE_CONTEXT_DATA_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    if (itorGpgpuCsrBaseAddress != cmdList.end()) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}

template <>
bool HardwareParse::requiresPipelineSelectBeforeMediaState<GenGfxFamily>() {
    return false;
}

} // namespace NEO
