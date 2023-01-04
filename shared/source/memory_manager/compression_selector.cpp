/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"

namespace NEO {

bool CompressionSelector::allowStatelessCompression() {
    if (!NEO::ApiSpecificConfig::isStatelessCompressionSupported()) {
        return false;
    }
    if (DebugManager.flags.EnableStatelessCompression.get() != -1) {
        return static_cast<bool>(DebugManager.flags.EnableStatelessCompression.get());
    }
    return false;
}

} // namespace NEO
