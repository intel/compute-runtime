/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

using namespace AubMemDump;

TEST(LrcaHelper, WhenLrcaHelperIsInitializedThenLrcaIncludesDebugModeLri) {
    LrcaHelper helper(0x2000);

    auto lrcaBufferSize = helper.sizeLRCA / sizeof(uint32_t);
    auto lrca = std::unique_ptr<uint32_t[]>(new uint32_t[lrcaBufferSize]);
    helper.initialize(lrca.get());

    bool debugModeLriFound = false;
    for (uint32_t i = 0; i < lrcaBufferSize; i += 2) {
        if (lrca[i] == 0x20d8 && lrca[i + 1] == 0x00200020) {
            debugModeLriFound = true;
            break;
        }
    }

    EXPECT_TRUE(debugModeLriFound);
}
