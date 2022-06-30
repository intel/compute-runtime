/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"
using GenStruct = NEO::XeHpCore;
using GenGfxFamily = NEO::XeHpFamily;
#include "shared/test/common/cmd_parse/cmd_parse_xehp_and_later.inl"

template const typename GenGfxFamily::RENDER_SURFACE_STATE *NEO::HardwareParse::getSurfaceState<GenGfxFamily>(IndirectHeap *ssh, uint32_t index);
