/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"

#include <cstdint>

namespace L0 {
inline uint64_t getIntermediateCacheSize(const NEO::HardwareInfo &hwInfo) {
    return 0u;
}
} // namespace L0
