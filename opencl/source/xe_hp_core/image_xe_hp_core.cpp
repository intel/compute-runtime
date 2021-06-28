/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = XeHpFamily;
static auto gfxCore = IGFX_XE_HP_CORE;

} // namespace NEO

#include "opencl/source/mem_obj/image_xehp_plus.inl"

namespace NEO {
// clang-format off
#include "opencl/source/mem_obj/image_tgllp_plus.inl"
#include "opencl/source/mem_obj/image_factory_init.inl"
// clang-format on
} // namespace NEO
