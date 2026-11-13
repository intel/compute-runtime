/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

bool CompressionSelector::isUsmCompressionAllowed() {
    return debugManager.flags.EnableUsmCompression.get() != 0;
}

} // namespace NEO
