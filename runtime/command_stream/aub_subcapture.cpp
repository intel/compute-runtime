/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
        subCaptureIsActive = isSubCaptureFilterActive(dispatchInfo);
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

bool AubSubCaptureManager::isKernelIndexInSubCaptureRange(uint32_t kernelIdx, uint32_t rangeStartIdx, uint32_t rangeEndIdx) const {
    return ((rangeStartIdx <= kernelIdx) && (kernelIdx <= rangeEndIdx));
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
        filterFileName += "_from_" + std::to_string(subCaptureFilter.dumpNamedKernelStartIdx);
        filterFileName += "_to_" + std::to_string(subCaptureFilter.dumpNamedKernelEndIdx);
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

bool AubSubCaptureManager::isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo) {
    auto kernelName = dispatchInfo.peekMainKernel()->getKernelInfo().name;
    auto subCaptureIsActive = false;

    if (subCaptureFilter.dumpKernelName.empty()) {
        if (isKernelIndexInSubCaptureRange(kernelCurrentIdx, subCaptureFilter.dumpKernelStartIdx, subCaptureFilter.dumpKernelEndIdx)) {
            subCaptureIsActive = true;
        }
    } else {
        if (0 == kernelName.compare(subCaptureFilter.dumpKernelName)) {
            if (isKernelIndexInSubCaptureRange(kernelNameMatchesNum, subCaptureFilter.dumpNamedKernelStartIdx, subCaptureFilter.dumpNamedKernelEndIdx)) {
                subCaptureIsActive = true;
            }
            kernelNameMatchesNum++;
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
