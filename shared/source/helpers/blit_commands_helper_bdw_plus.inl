/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper_base.inl"

namespace NEO {

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, typename GfxFamily::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {}

} // namespace NEO
