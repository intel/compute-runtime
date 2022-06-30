/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {

bool PreemptionHelper::allowThreadGroupPreemption(const PreemptionFlags &flags) {
    if (flags.flags.disablePerCtxtPreemptionGranularityControl) {
        return false;
    }

    if (flags.flags.usesFencesForReadWriteImages &&
        flags.flags.disableLSQCROPERFforOCL) {
        return false;
    }
    if (flags.flags.vmeKernel) {
        return false;
    }

    return true;
}

bool PreemptionHelper::allowMidThreadPreemption(const PreemptionFlags &flags) {
    return (flags.flags.disabledMidThreadPreemptionKernel == 0) &&
           !(flags.flags.vmeKernel && !flags.flags.deviceSupportsVmePreemption);
}

PreemptionMode PreemptionHelper::taskPreemptionMode(PreemptionMode devicePreemptionMode, const PreemptionFlags &flags) {
    if (DebugManager.flags.ForceKernelPreemptionMode.get() != -1) {
        return static_cast<PreemptionMode>(DebugManager.flags.ForceKernelPreemptionMode.get());
    }
    if (devicePreemptionMode == PreemptionMode::Disabled) {
        return PreemptionMode::Disabled;
    }

    if (devicePreemptionMode >= PreemptionMode::MidThread &&
        allowMidThreadPreemption(flags)) {
        return PreemptionMode::MidThread;
    }

    if (devicePreemptionMode >= PreemptionMode::ThreadGroup &&
        allowThreadGroupPreemption(flags)) {
        return PreemptionMode::ThreadGroup;
    }

    return PreemptionMode::MidBatch;
};

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

PreemptionFlags PreemptionHelper::createPreemptionLevelFlags(Device &device, const KernelDescriptor *kernelDescriptor) {
    PreemptionFlags flags = {};
    if (kernelDescriptor) {
        flags.flags.disabledMidThreadPreemptionKernel = kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption;
        flags.flags.vmeKernel = kernelDescriptor->kernelAttributes.flags.usesVme;
        flags.flags.usesFencesForReadWriteImages = kernelDescriptor->kernelAttributes.flags.usesFencesForReadWriteImages;
    }
    flags.flags.deviceSupportsVmePreemption = device.getDeviceInfo().vmeAvcSupportsPreemption;
    flags.flags.disablePerCtxtPreemptionGranularityControl = device.getHardwareInfo().workaroundTable.flags.waDisablePerCtxtPreemptionGranularityControl;
    flags.flags.disableLSQCROPERFforOCL = device.getHardwareInfo().workaroundTable.flags.waDisableLSQCROPERFforOCL;
    return flags;
}

} // namespace NEO
