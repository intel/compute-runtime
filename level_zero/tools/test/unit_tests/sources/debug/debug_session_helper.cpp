/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32
namespace NEO {
class EuDebugInterface {};
} // namespace NEO
#else
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"
#endif

#include "level_zero/tools/test/unit_tests/sources/debug/mock_debug_session.h"
#include <level_zero/ze_api.h>

namespace L0 {

namespace ult {
CreateDebugSessionHelperFunc createDebugSessionFunc = nullptr;
CreateDebugSessionHelperFunc createDebugSessionFuncXe = nullptr;
} // namespace ult
DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd, void *params) {
    if (L0::ult::createDebugSessionFunc) {
        return L0::ult::createDebugSessionFunc(config, device, debugFd, params);
    }
    return new L0::ult::DebugSessionMock(config, device);
}

DebugSession *createDebugSessionHelperXe(const zet_debug_config_t &config, Device *device, int debugFd, std::unique_ptr<NEO::EuDebugInterface> debugInterface, void *params) {
    if (L0::ult::createDebugSessionFuncXe) {
        return L0::ult::createDebugSessionFuncXe(config, device, debugFd, params);
    }
    return new L0::ult::DebugSessionMock(config, device);
}

} // namespace L0