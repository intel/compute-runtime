/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_l0_debugger.h"

namespace L0 {
namespace ult {
DebugerL0CreateFn mockDebuggerL0HwFactory[IGFX_MAX_CORE];
}
} // namespace L0

namespace L0 {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    return std::unique_ptr<DebuggerL0>(ult::mockDebuggerL0HwFactory[device->getHardwareInfo().platform.eRenderCoreFamily](device));
}

} // namespace L0