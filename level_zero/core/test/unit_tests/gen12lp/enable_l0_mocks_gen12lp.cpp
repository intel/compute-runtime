/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_l0_debugger.h"

namespace NEO {

struct TGLLPFamily;
using GfxFamily = TGLLPFamily;

} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_GEN12LP_CORE, NEO::GfxFamily> mockDebuggerGen12lp;
}
} // namespace L0