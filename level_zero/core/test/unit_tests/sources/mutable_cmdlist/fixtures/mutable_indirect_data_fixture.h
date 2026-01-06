/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/mocks/mock_mutable_indirect_data.h"

#include <functional>
#include <memory>

namespace L0 {
namespace ult {

struct MutableIndirectDataFixture {
    void setUp() {}
    void tearDown() {}

    void createMutableIndirectOffset();
    void recalculateOffsets(L0::MCL::CrossThreadDataOffset initialOffset);
    void undefineOffsets();

    struct Offsets {
        constexpr static L0::MCL::CrossThreadDataOffset patchSize = 12;
        constexpr static L0::MCL::CrossThreadDataOffset initialOffset = 16;

        L0::MCL::CrossThreadDataOffset globalWorkSize = initialOffset;
        L0::MCL::CrossThreadDataOffset localWorkSize = initialOffset + patchSize;
        L0::MCL::CrossThreadDataOffset localWorkSize2 = initialOffset + 2 * patchSize;
        L0::MCL::CrossThreadDataOffset enqLocalWorkSize = initialOffset + 3 * patchSize;
        L0::MCL::CrossThreadDataOffset numWorkGroups = initialOffset + 4 * patchSize;
        L0::MCL::CrossThreadDataOffset workDimensions = initialOffset + 5 * patchSize;
        L0::MCL::CrossThreadDataOffset globalWorkOffset = initialOffset + 6 * patchSize;
    } offsets;

    std::unique_ptr<L0::MCL::MutableIndirectData> indirectData;
    std::unique_ptr<uint8_t, std::function<void(void *)>> perThreadData;
    std::unique_ptr<uint8_t, std::function<void(void *)>> crossThreadData;
    std::unique_ptr<uint8_t, std::function<void(void *)>> inlineData;

    MockMutableIndirectData *mockMutableIndirectData = nullptr;

    size_t perThreadDataSize = 0;
    size_t crossThreadDataSize = 4096;
    size_t inlineSize = 0;

    L0::MCL::CrossThreadDataOffset undefinedOffset = L0::MCL::undefined<L0::MCL::CrossThreadDataOffset>;
};

} // namespace ult
} // namespace L0
