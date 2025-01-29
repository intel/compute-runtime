/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/mock_sip_listener.h"

#include "shared/source/memory_manager/memory_allocation.h"

#include "gtest/gtest.h"

namespace NEO {

void MockSipListener::OnTestEnd(const testing::TestInfo &testInfo) {
    if (MockSipData::mockSipKernel) {
        if (MockSipData::mockSipKernel->tempSipMemoryAllocation) {
            printf("*** WARNING: test did not free sip kernels, test name: %s ***\n", testInfo.name());
            MockSipData::mockSipKernel->tempSipMemoryAllocation.reset(nullptr);
        }
    }
}

} // namespace NEO
