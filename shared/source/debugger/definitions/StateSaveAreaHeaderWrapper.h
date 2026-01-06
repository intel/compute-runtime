/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common/StateSaveAreaHeader.h"

#include <cstdint>

namespace NEO {
// NOLINTBEGIN
struct StateSaveAreaHeader {
    struct SIP::StateSaveArea versionHeader;
    union {
        struct SIP::intelgt_state_save_area regHeader;
        struct SIP::intelgt_state_save_area_V3 regHeaderV3;
        uint64_t totalWmtpDataSize;
    };
};

union SipCommandRegisterValues {
    SIP::sip_command sip_commandValues;
};

// NOLINTEND
} // namespace NEO
