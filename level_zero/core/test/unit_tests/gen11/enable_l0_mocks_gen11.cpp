/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_l0_debugger.h"

namespace NEO {

struct ICLFamily;
using GfxFamily = ICLFamily;
} // namespace NEO

namespace L0 {
namespace ult {
static MockDebuggerL0HwPopulateFactory<IGFX_GEN11_CORE, NEO::GfxFamily> mockDebuggerGen11;
}
} // namespace L0
