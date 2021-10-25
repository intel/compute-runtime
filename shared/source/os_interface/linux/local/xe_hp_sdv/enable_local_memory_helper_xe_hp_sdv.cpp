/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

namespace NEO {

static EnableProductLocalMemoryHelper<IGFX_XE_HP_SDV> enableLocalMemHelperXeHpSdv;

} // namespace NEO
