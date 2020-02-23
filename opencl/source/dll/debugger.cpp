/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debugger/debugger.h"

#include "built_ins/sip_kernel_type.h"
#include "helpers/hw_info.h"
#include "opencl/source/built_ins/sip.h"
#include "opencl/source/source_level_debugger/source_level_debugger.h"

namespace NEO {
std::unique_ptr<Debugger> Debugger::create(HardwareInfo *hwInfo) {
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
    if (hwInfo->capabilityTable.debuggerSupported) {
        sourceLevelDebugger.reset(SourceLevelDebugger::create());
    }
    if (sourceLevelDebugger) {
        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == SipKernel::getSipKernelType(hwInfo->platform.eRenderCoreFamily, true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);
    }
    return sourceLevelDebugger;
}
} // namespace NEO