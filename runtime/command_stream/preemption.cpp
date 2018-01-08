/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/string.h"
#include "runtime/kernel/kernel.h"

namespace OCLRT {

bool PreemptionHelper::allowThreadGroupPreemption(Kernel *kernel, const WorkaroundTable *waTable) {
    if (waTable->waDisablePerCtxtPreemptionGranularityControl) {
        return false;
    }

    if (kernel) {
        if (kernel->getKernelInfo().patchInfo.executionEnvironment &&
            kernel->getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages &&
            waTable->waDisableLSQCROPERFforOCL) {
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
        allowThreadGroupPreemption(kernel, device.getWaTable())) {
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

size_t PreemptionHelper::getInstructionHeapSipKernelReservedSize(const Device &device) {
    if (device.getPreemptionMode() != PreemptionMode::MidThread) {
        return 0;
    }

    return BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, device).getBinarySize();
}

void PreemptionHelper::initializeInstructionHeapSipKernelReservedBlock(LinearStream &ih, const Device &device) {
    if (device.getPreemptionMode() != PreemptionMode::MidThread) {
        return;
    }

    const SipKernel &sip = BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, device);
    size_t sipSize = sip.getBinarySize();
    UNRECOVERABLE_IF(sipSize > ih.getAvailableSpace());
    UNRECOVERABLE_IF(0 != ih.getUsed());
    void *blockForSip = ih.getSpace(sipSize);
    UNRECOVERABLE_IF(nullptr == blockForSip);
    auto err = memcpy_s(blockForSip, sipSize, sip.getBinary(), sipSize);
    UNRECOVERABLE_IF(err != 0);
}

// verify that SIP CSR kernel resides at the begining of the InstructionHeap
bool PreemptionHelper::isValidInstructionHeapForMidThreadPreemption(const LinearStream &ih, const Device &device) {
    const SipKernel &sip = BuiltIns::getInstance().getSipKernel(SipKernelType::Csr, device);
    if (ih.getUsed() < sip.getBinarySize()) {
        return false;
    }

    return (0 == memcmp(ih.getBase(), sip.getBinary(), sip.getBinarySize()));
}

} // namespace OCLRT
