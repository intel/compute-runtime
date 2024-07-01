/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/debug_session_imp.h"

namespace L0 {

ze_result_t DebugSessionImp::readThreadScratchRegisters(EuThread::ThreadId threadId, uint32_t start, uint32_t count, void *pRegisterValues) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0