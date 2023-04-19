/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

namespace NEO {

struct MemAdviseFlags {
    union {
        uint8_t allFlags; /* all memAdvise flags */
        struct
        {
            uint8_t readOnly : 1;                /* ZE_MEMORY_ADVICE_SET_READ_MOSTLY or ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY */
            uint8_t devicePreferredLocation : 1; /* ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION  or ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION  */
            uint8_t nonAtomic : 1;               /* ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY  or ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY  */
            uint8_t cachedMemory : 1;            /* ZE_MEMORY_ADVICE_BIAS_CACHED or ZE_MEMORY_ADVICE_BIAS_UNCACHED */
            uint8_t cpuMigrationBlocked : 1;     /* ZE_MEMORY_ADVICE_SET_READ_MOSTLY and ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION */
            uint8_t reserved2 : 1;
            uint8_t reserved1 : 1;
            uint8_t reserved0 : 1;
        };
    };
    MemAdviseFlags() {
        allFlags = 0;
        devicePreferredLocation = 1;
        cachedMemory = 1;
    }
};
static_assert(sizeof(MemAdviseFlags) == sizeof(uint8_t), "");
} // namespace NEO
