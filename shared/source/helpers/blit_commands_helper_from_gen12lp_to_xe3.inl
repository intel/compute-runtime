/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_properties.h"

namespace NEO {

template <typename GfxFamily>
template <typename CommandType>
void BlitCommandsHelper<GfxFamily>::applyAdditionalBlitProperties(const BlitProperties &blitProperties,
                                                                  CommandType &cmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last) {
}

} // namespace NEO
