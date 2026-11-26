/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_aub_manager.h"

namespace NEO {
bool createAubManagerReturnsNullptr = false;

aub_stream::AubManager *createAubManager(const aub_stream::AubManagerOptions &options) {
    if (createAubManagerReturnsNullptr) {
        return nullptr;
    }
    return new MockAubManager(options);
}
} // namespace NEO
