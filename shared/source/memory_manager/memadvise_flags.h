/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

namespace NEO {

typedef union MemAdviseFlagsTag {
    uint8_t memadvise_flags; /* all memadvise_flags */
    struct
    {
        uint8_t read_only : 1,             /* ZE_MEMORY_ADVICE_SET_READ_MOSTLY or ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY */
            device_preferred_location : 1, /* ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION  or ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION  */
            non_atomic : 1,                /* ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY  or ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY  */
            cached_memory : 1,             /* ZE_MEMORY_ADVICE_BIAS_CACHED or ZE_MEMORY_ADVICE_BIAS_UNCACHED */
            cpu_migration_blocked : 1,     /* ZE_MEMORY_ADVICE_SET_READ_MOSTLY and ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION */
            reserved2 : 1,
            reserved1 : 1,
            reserved0 : 1;
    };
    MemAdviseFlagsTag() {
        memadvise_flags = 0;
        cached_memory = 1;
    }
} MemAdviseFlags;

} // namespace NEO
