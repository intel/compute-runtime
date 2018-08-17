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

AubSubCaptureManager::AubSubCaptureManager(const std::string &fileName)
    : initialFileName(fileName) {
    settingsReader.reset(SettingsReader::createOsReader(true));
}

AubSubCaptureManager::~AubSubCaptureManager() = default;

bool AubSubCaptureManager::activateSubCapture(const MultiDispatchInfo &dispatchInfo) {
    if (dispatchInfo.empty()) {
        return false;
    }

    subCaptureWasActive = subCaptureIsActive;
    subCaptureIsActive = false;

    switch (subCaptureMode) {
    case SubCaptureMode::Toggle:
        subCaptureIsActive = isSubCaptureToggleActive();
        break;
    case SubCaptureMode::Filter:
        subCaptureIsActive = isSubCaptureFilterActive(dispatchInfo, kernelCurrentIdx);
        break;
    default:
        DEBUG_BREAK_IF(false);
        break;
    }

    kernelCurrentIdx++;
    setDebugManagerFlags();

    return subCaptureIsActive;
}

const std::string &AubSubCaptureManager::getSubCaptureFileName(const MultiDispatchInfo &dispatchInfo) {
    if (useExternalFileName) {
        currentFileName = getExternalFileName();
    }
    switch (subCaptureMode) {
    case SubCaptureMode::Filter:
        if (currentFileName.empty()) {
            currentFileName = generateFilterFileName();
            useExternalFileName = false;
        }
        break;
    case SubCaptureMode::Toggle:
        if (currentFileName.empty()) {
            currentFileName = generateToggleFileName(dispatchInfo);
            useExternalFileName = false;
        }
        break;
    default:
        DEBUG_BREAK_IF(false);
        break;
    }

    return currentFileName;
}

bool AubSubCaptureManager::isKernelIndexInSubCaptureRange(uint32_t kernelIdx) const {
    return ((subCaptureFilter.dumpKernelStartIdx <= kernelIdx) &&
            (kernelIdx <= subCaptureFilter.dumpKernelEndIdx));
}

bool AubSubCaptureManager::isSubCaptureToggleActive() const {
    return settingsReader->getSetting("AUBDumpToggleCaptureOnOff", false);
}

std::string AubSubCaptureManager::getExternalFileName() const {
    return settingsReader->getSetting("AUBDumpToggleFileName", std::string(""));
}

std::string AubSubCaptureManager::generateFilterFileName() const {
    std::string baseFileName = initialFileName.substr(0, initialFileName.length() - strlen(".aub"));
    std::string filterFileName = baseFileName + "_filter";
    filterFileName += "_from_" + std::to_string(subCaptureFilter.dumpKernelStartIdx);
    filterFileName += "_to_" + std::to_string(subCaptureFilter.dumpKernelEndIdx);
    if (!subCaptureFilter.dumpKernelName.empty()) {
        filterFileName += "_" + subCaptureFilter.dumpKernelName;
    }
    filterFileName += ".aub";
    return filterFileName;
}

std::string AubSubCaptureManager::generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const {
    std::string baseFileName = initialFileName.substr(0, initialFileName.length() - strlen(".aub"));
    std::string toggleFileName = baseFileName + "_toggle";
    toggleFileName += "_from_" + std::to_string(kernelCurrentIdx - 1);
    if (!dispatchInfo.empty()) {
        toggleFileName += "_" + dispatchInfo.peekMainKernel()->getKernelInfo().name;
    }
    toggleFileName += ".aub";
    return toggleFileName;
}

bool AubSubCaptureManager::isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo, uint32_t kernelIdx) const {
    DEBUG_BREAK_IF(dispatchInfo.size() > 1);
    auto kernelName = dispatchInfo.peekMainKernel()->getKernelInfo().name;
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
