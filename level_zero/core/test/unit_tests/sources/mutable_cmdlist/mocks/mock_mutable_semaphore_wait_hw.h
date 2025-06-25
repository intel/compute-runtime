/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <typename GfxFamily>
struct WhiteBox<::L0::MCL::MutableSemaphoreWaitHw<GfxFamily>>
    : public ::L0::MCL::MutableSemaphoreWaitHw<GfxFamily> {

    using BaseClass = ::L0::MCL::MutableSemaphoreWaitHw<GfxFamily>;
    using BaseClass::inOrderPatchListIndex;
    using BaseClass::offset;
    using BaseClass::qwordDataIndirect;
    using BaseClass::semWait;
    using BaseClass::type;
};

template <typename GfxFamily>
using MockMutableSemaphoreWaitHw = WhiteBox<::L0::MCL::MutableSemaphoreWaitHw<GfxFamily>>;

} // namespace ult
} // namespace L0
