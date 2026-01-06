/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <typename GfxFamily>
struct WhiteBox<::L0::MCL::MutableLoadRegisterImmHw<GfxFamily>>
    : public ::L0::MCL::MutableLoadRegisterImmHw<GfxFamily> {

    using BaseClass = ::L0::MCL::MutableLoadRegisterImmHw<GfxFamily>;
    using BaseClass::loadRegImm;
    using BaseClass::registerAddress;
};

template <typename GfxFamily>
using MockMutableLoadRegisterImmHw = WhiteBox<::L0::MCL::MutableLoadRegisterImmHw<GfxFamily>>;

} // namespace ult
} // namespace L0
