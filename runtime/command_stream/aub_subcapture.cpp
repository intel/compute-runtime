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

#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"
#include "runtime/utilities/debug_settings_reader.h"

namespace OCLRT {

AubSubCaptureManager::AubSubCaptureManager() {
    settingsReader.reset(SettingsReader::createOsReader(true));
}

AubSubCaptureManager::~AubSubCaptureManager() = default;

void AubSubCaptureManager::activateSubCapture(const MultiDispatchInfo &dispatchInfo) {
    if (dispatchInfo.empty()) {
        return;
    }

    subCaptureWasActive = subCaptureIsActive;
    subCaptureIsActive = false;

    switch (subCaptureMode) {
    case SubCaptureMode::Toggle:
        subCaptureIsActive = isSubCaptureToggleActive();
        break;
    case SubCaptureMode::Filter:
        subCaptureIsActive = isSubCaptureFilterActive(dispatchInfo, kernelCurrentIdx);
        kernelCurrentIdx++;
        break;
    default:
        DEBUG_BREAK_IF(false);
        break;
    }

    setDebugManagerFlags();
}

void AubSubCaptureManager::deactivateSubCapture() {
    subCaptureWasActive = false;
    subCaptureIsActive = false;
}

bool AubSubCaptureManager::isKernelIndexInSubCaptureRange(uint32_t kernelIdx) const {
    return ((subCaptureFilter.dumpKernelStartIdx <= kernelIdx) &&
            (kernelIdx <= subCaptureFilter.dumpKernelEndIdx));
}

bool AubSubCaptureManager::isSubCaptureToggleActive() const {
    return settingsReader->getSetting("AUBDumpToggleCaptureOnOff", false);
}

bool AubSubCaptureManager::isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo, uint32_t kernelIdx) const {
    DEBUG_BREAK_IF(dispatchInfo.size() > 1);
    auto kernelName = dispatchInfo.begin()->getKernel()->getKernelInfo().name;
    auto subCaptureIsActive = false;

    if (isKernelIndexInSubCaptureRange(kernelIdx)) {
        if (subCaptureFilter.dumpKernelName.empty()) {
            subCaptureIsActive = true;
        } else {
            if (0 == kernelName.compare(subCaptureFilter.dumpKernelName)) {
                subCaptureIsActive = true;
            }
        }
    }
    return subCaptureIsActive;
}

void AubSubCaptureManager::setDebugManagerFlags() const {
    DebugManager.flags.MakeEachEnqueueBlocking.set(!subCaptureIsActive);
    DebugManager.flags.ForceCsrFlushing.set(false);
    if (!subCaptureIsActive && subCaptureWasActive) {
        DebugManager.flags.ForceCsrFlushing.set(true);
    }
    DebugManager.flags.ForceCsrReprogramming.set(false);
    if (subCaptureIsActive && !subCaptureWasActive) {
        DebugManager.flags.ForceCsrReprogramming.set(true);
    }
}
} // namespace OCLRT
