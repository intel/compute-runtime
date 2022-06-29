/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
using GenStruct = NEO::XE_HPG_CORE;
using GenGfxFamily = NEO::XE_HPG_COREFamily;
#include "shared/test/common/cmd_parse/cmd_parse_xehp_and_later.inl"

template const typename GenGfxFamily::RENDER_SURFACE_STATE *NEO::HardwareParse::getSurfaceState<GenGfxFamily>(IndirectHeap *ssh, uint32_t index);