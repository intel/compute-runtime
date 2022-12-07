/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isZebinAllowed(const NEO::SourceLevelDebugger *debugger) const {
    return true;
}

} // namespace L0
