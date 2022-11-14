/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger.h"

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

namespace NEO {
std::unique_ptr<Debugger> Debugger::create(const RootDeviceEnvironment &rootDeviceEnvironment) {
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    if (hwInfo.capabilityTable.debuggerSupported || DebugManager.flags.ExperimentalEnableSourceLevelDebugger.get()) {
        sourceLevelDebugger.reset(SourceLevelDebugger::create());
    }
    if (sourceLevelDebugger) {
        auto &coreHelper = rootDeviceEnvironment.getHelper<CoreHelper>();

        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == coreHelper.getSipKernelType(true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);

        if (sourceLevelDebugger->isDebuggerActive()) {
            hwInfo.capabilityTable.fusedEuEnabled = false;
        }
    }
    return sourceLevelDebugger;
}

void *Debugger::getDebugSurfaceReservedSurfaceState(IndirectHeap &ssh) {
    return ssh.getCpuBase();
}
} // namespace NEO