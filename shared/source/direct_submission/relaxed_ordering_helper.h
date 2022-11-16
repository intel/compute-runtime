/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_alu_helper.h"

namespace NEO {
namespace RelaxedOrderingHelper {

template <typename GfxFamily>
constexpr size_t getSizeTaskStoreSection() {
    return ((6 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM)) +
            EncodeAluHelper<GfxFamily, 9>::getCmdsSize() +
            EncodeMathMMIO<GfxFamily>::getCmdSizeForIncrementOrDecrement() +
            EncodeMiPredicate<GfxFamily>::getCmdSize());
}

} // namespace RelaxedOrderingHelper
} // namespace NEO