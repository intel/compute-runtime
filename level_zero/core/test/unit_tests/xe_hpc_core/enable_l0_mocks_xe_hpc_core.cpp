/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_l0_debugger.h"

namespace NEO {
struct XeHpcCoreFamily;
using GfxFamily = XeHpcCoreFamily;
} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_XE_HPC_CORE, NEO::GfxFamily> mockDebuggerXeHpcCore;
}
} // namespace L0
