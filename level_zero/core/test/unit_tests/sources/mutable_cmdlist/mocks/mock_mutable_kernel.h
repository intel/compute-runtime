/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_kernel.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::MCL::MutableKernel>
    : public ::L0::MCL::MutableKernel {

    using BaseClass = ::L0::MCL::MutableKernel;
    using BaseClass::computeWalker;
    using BaseClass::hostViewIndirectData;
    using BaseClass::hostViewIndirectHeap;
    using BaseClass::inlineDataSize;
    using BaseClass::kernel;
    using BaseClass::kernelDispatch;
    using BaseClass::kernelResidencySnapshotContainer;
    using BaseClass::kernelVariables;
    using BaseClass::maxPerThreadDataSize;
    using BaseClass::regionBarrierSnapshotResidencyIndex;
    using BaseClass::syncBufferSnapshotResidencyIndex;
};

using MockMutableKernel = WhiteBox<::L0::MCL::MutableKernel>;

} // namespace ult
} // namespace L0
