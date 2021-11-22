/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_l0_debugger.h"

namespace NEO {
struct XE_HPG_COREFamily;
using GfxFamily = XE_HPG_COREFamily;
} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_XE_HPG_CORE, NEO::GfxFamily> mockDebuggerXeHpgCore;
}
} // namespace L0