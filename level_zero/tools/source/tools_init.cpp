/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tools_init.h"

#include "level_zero/tools/source/tools_init_imp.h"

namespace L0 {
static ToolsInitImp toolsInitImp;
ToolsInit *ToolsInit::toolsInit = &toolsInitImp;
} // namespace L0
