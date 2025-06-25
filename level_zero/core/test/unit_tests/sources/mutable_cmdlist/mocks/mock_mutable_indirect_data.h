/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_indirect_data.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::MCL::MutableIndirectData>
    : public ::L0::MCL::MutableIndirectData {

    using BaseClass = ::L0::MCL::MutableIndirectData;
    using BaseClass::crossThreadData;
    using BaseClass::inlineData;
    using BaseClass::offsetInHeap;
    using BaseClass::offsets;
    using BaseClass::perThreadData;
};

using MockMutableIndirectData = WhiteBox<::L0::MCL::MutableIndirectData>;

} // namespace ult
} // namespace L0
