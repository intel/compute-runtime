/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/gfx_escape_wrapper.h"

#include <cstdint>

namespace NEO {

struct GfxSynchPartitionInfoEscapeHeader {
    GFX_ESCAPE_HEADER_T base;
    uint32_t action;
    uint32_t padding;

    static constexpr uint32_t gmmEscapeCode = 2; // GFX_ESCAPE_GMM_CONTROL
    static constexpr uint32_t actionGet = 28;    // GMM_ESCAPE_ACTION_GET_PROCESS_GFX_PARTITION
    static constexpr uint32_t actionSet = 29;    // GMM_ESCAPE_ACTION_SET_PROCESS_GFX_PARTITION
    static constexpr uint32_t escapeDataTailPaddingInBytes = sizeof(uint32_t) * 2;
};

} // namespace NEO
