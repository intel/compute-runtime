/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"

#include "encode_surface_state_args.h"
#include "implicit_args.h"
#include "neo_igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct KernelHw : public KernelImp {
    using KernelImp::KernelImp;
};

} // namespace L0
