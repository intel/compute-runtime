/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"

#include "core/helpers/string.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/device/device.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"

namespace NEO {

bool PreemptionHelper::allowThreadGroupPreemption(Kernel *kernel, const WorkaroundTable *workaroundTable) {
    if (workaroundTable->waDisablePerCtxtPreemptionGranularityControl) {
        return false;
    }

    if (kernel) {
        if (kernel->getKernelInfo().patchInfo.executionEnvironment &&
            kernel->getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages &&
            workaroundTable->waDisableLSQCROPERFforOCL) {
            return false;
        }
        if (kernel->isSchedulerKernel || kernel->isVmeKernel()) {
            return false;
        }
    }

    return true;
}

bool PreemptionHelper::allowMidThreadPreemption(Kernel *kernel, Device &device) {
    bool allowedByKernel = true;
    if (kernel) {
        allowedByKernel = (kernel->getKernelInfo().patchInfo.executionEnvironment->DisableMidThreadPreemption == 0) &&
                          !(kernel->isVmeKernel() && !device.getDeviceInfo().vmeAvcSupportsPreemption);
    }
    bool supportedByDevice = (device.getPreemptionMode() >= PreemptionMode::MidThread);
    return supportedByDevice && allowedByKernel;
}

PreemptionMode PreemptionHelper::taskPreemptionMode(Device &device, Kernel *kernel) {
    if (device.getPreemptionMode() == PreemptionMode::Disabled) {
        return PreemptionMode::Disabled;
    }

    if (device.getPreemptionMode() >= PreemptionMode::MidThread &&
        allowMidThreadPreemption(kernel, device)) {
        return PreemptionMode::MidThread;
    }

    if (device.getPreemptionMode() >= PreemptionMode::ThreadGroup &&
        allowThreadGroupPreemption(kernel, &device.getHardwareInfo().workaroundTable)) {
        return PreemptionMode::ThreadGroup;
    }

    return PreemptionMode::MidBatch;
};

PreemptionMode PreemptionHelper::taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo) {
    PreemptionMode devMode = device.getPreemptionMode();

    for (const auto &di : multiDispatchInfo) {
        PreemptionMode taskMode = taskPreemptionMode(device, di.getKernel());
        if (devMode > taskMode) {
            devMode = taskMode;
        }
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stdout, "devMode = %d, taskMode = %d.\n",
                         static_cast<int>(device.getPreemptionMode()), static_cast<int>(taskMode));
    }
    return devMode;
}

void PreemptionHelper::adjustDefaultPreemptionMode(RuntimeCapabilityTable &deviceCapabilities, bool allowMidThread, bool allowThreadGroup, bool allowMidBatch) {
    if (deviceCapabilities.defaultPreemptionMode >= PreemptionMode::MidThread &&
        allowMidThread) {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;
    } else if (deviceCapabilities.defaultPreemptionMode >= PreemptionMode::ThreadGroup &&
               allowThreadGroup) {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    } else if (deviceCapabilities.defaultPreemptionMode >= PreemptionMode::MidBatch &&
               allowMidBatch) {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::MidBatch;
    } else {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::Disabled;
    }
}

PreemptionMode PreemptionHelper::getDefaultPreemptionMode(const HardwareInfo &hwInfo) {
    return DebugManager.flags.ForcePreemptionMode.get() == -1
               ? hwInfo.capabilityTable.defaultPreemptionMode
               : static_cast<PreemptionMode>(DebugManager.flags.ForcePreemptionMode.get());
}

} // namespace NEO
