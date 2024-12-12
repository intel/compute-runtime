/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_upstream.h"

namespace NEO {
[[maybe_unused]] static EnableEuDebugInterface enableEuDebugUpstream(EuDebugInterfaceType::upstream, EuDebugInterfaceUpstream::sysFsXeEuDebugFile, []() -> std::unique_ptr<EuDebugInterface> { return std::make_unique<EuDebugInterfaceUpstream>(); });
}
