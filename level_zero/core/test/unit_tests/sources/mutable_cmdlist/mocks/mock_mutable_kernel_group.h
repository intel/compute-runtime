/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_group.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::MCL::MutableKernelGroup>
    : public ::L0::MCL::MutableKernelGroup {

    using BaseClass = ::L0::MCL::MutableKernelGroup;
    using BaseClass::currentMutableKernel;
    using BaseClass::iohForPrefetch;
    using BaseClass::kernelsInAppend;
    using BaseClass::maxAppendIndirectHeapSize;
    using BaseClass::maxAppendScratchSize;
    using BaseClass::maxIsaSize;
    using BaseClass::prefetchCmd;
};

using MockMutableKernelGroup = WhiteBox<::L0::MCL::MutableKernelGroup>;

} // namespace ult
} // namespace L0
