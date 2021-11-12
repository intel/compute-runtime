/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include "gmock/gmock.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Fence> : public ::L0::Fence {
    using ::L0::Fence::allocation;
    using ::L0::Fence::partitionCount;
};

using Fence = WhiteBox<::L0::Fence>;

template <>
struct Mock<Fence> : public Fence {
    Mock() : mockAllocation(0, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
                            &memory, reinterpret_cast<uint64_t>(&memory), 0, sizeof(memory),
                            MemoryPool::System4KBPages) {
        allocation = &mockAllocation;
    }
    ~Mock() override = default;

    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(hostSynchronize, ze_result_t, ZE_RESULT_SUCCESS, (uint64_t timeout));
    ADDMETHOD_NOBASE(queryStatus, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(reset, ze_result_t, ZE_RESULT_SUCCESS, ());

    // Fake an allocation for event memory
    alignas(16) uint32_t memory = -1;
    NEO::GraphicsAllocation mockAllocation;
};

} // namespace ult
} // namespace L0
