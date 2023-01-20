/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"

namespace NEO {
aub_stream::AubManager *createAubManager(const aub_stream::AubManagerOptions &options) {
    return aub_stream::AubManager::create(options);
}
} // namespace NEO
