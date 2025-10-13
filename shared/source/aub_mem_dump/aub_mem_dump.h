/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
namespace AubMemDump {

struct LrcaHelper {
    static void setContextSaveRestoreFlags(uint32_t &value);
};

} // namespace AubMemDump
