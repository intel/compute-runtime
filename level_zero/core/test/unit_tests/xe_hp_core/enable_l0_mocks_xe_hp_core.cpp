/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_l0_debugger.h"

namespace NEO {
struct XeHpFamily;
using GfxFamily = XeHpFamily;
} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_XE_HP_CORE, NEO::GfxFamily> mockDebuggerXE_HP_CORE;
}
} // namespace L0
