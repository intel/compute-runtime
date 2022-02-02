/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_info.h"

#include "hw_cmds.h"

namespace NEO {

template <typename GfxFamily>
void EncodeEnableRayTracing<GfxFamily>::programEnableRayTracing(LinearStream &commandStream, GraphicsAllocation &backBuffer) {
}

} // namespace NEO
