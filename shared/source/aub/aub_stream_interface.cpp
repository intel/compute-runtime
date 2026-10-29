/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/aub_manager.h"

namespace NEO {
aub_stream::AubManager *createAubManager(const aub_stream::AubManagerOptions &options) {
    return aub_stream::AubManager::create(options);
}
} // namespace NEO
