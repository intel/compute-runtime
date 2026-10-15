/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class DeviceBinaryFormat : uint8_t {
    unknown,
    oclElf,
    oclLibrary,
    oclCompiledObject,
    archive,
    zebin
};

} // namespace NEO
