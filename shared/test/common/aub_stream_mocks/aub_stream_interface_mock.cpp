/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_aub_manager.h"

namespace NEO {
aub_stream::AubManager *createAubManager(const aub_stream::AubManagerOptions &options) {
    return new MockAubManager(options);
}
} // namespace NEO
