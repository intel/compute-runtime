/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include <level_zero/ze_api.h>

namespace L0 {

namespace ult {
CreateDebugSessionHelperFunc createDebugSessionFunc = nullptr;
}
DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd) {
    if (L0::ult::createDebugSessionFunc) {
        return L0::ult::createDebugSessionFunc(config, device, debugFd);
    }
    return new L0::ult::DebugSessionMock(config, device);
}

} // namespace L0