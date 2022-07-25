/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_l0_debugger.h"

namespace NEO {
struct Gen9Family;
using GfxFamily = Gen9Family;
} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_GEN9_CORE, NEO::GfxFamily> mockDebuggerGen9;
}
} // namespace L0