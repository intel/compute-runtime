/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/image/image_surface_state.h"

namespace NEO {

template <typename GfxFamily>
bool ImageSurfaceStateHelper<GfxFamily>::imageAsArrayWithArraySizeOf1NotPreferred() {
    return false;
}

} // namespace NEO
