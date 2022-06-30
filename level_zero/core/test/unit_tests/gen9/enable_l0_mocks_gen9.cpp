/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_l0_debugger.h"

namespace NEO {
struct SKLFamily;
using GfxFamily = SKLFamily;
} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_GEN9_CORE, NEO::GfxFamily> mockDebuggerGen9;
}
} // namespace L0