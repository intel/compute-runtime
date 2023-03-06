/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"

namespace NEO {
template <typename GfxFamily>
struct MockEncodeMiFlushDW : EncodeMiFlushDW<GfxFamily> {
    using EncodeMiFlushDW<GfxFamily>::getWaSize;
};
} // namespace NEO