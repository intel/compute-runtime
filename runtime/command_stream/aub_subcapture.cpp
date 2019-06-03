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

namespace NEO {

AubSubCaptureManager::AubSubCaptureManager(const std::string &fileName, AubSubCaptureCommon &subCaptureCommon)
    : initialFileName(fileName), subCaptureCommon(subCaptureCommon) {
    settingsReader.reset(SettingsReader::createOsReader(true));
}

AubSubCaptureManager::~AubSubCaptureManager() = default;

bool AubSubCaptureManager::isSubCaptureEnabled() const {
    auto guard = this->lock();

    return subCaptureIsActive || subCaptureWasActiveInPreviousEnqueue;
}

void AubSubCaptureManager::disableSubCapture() {
    auto guard = this->lock();

    subCaptureIsActive = subCaptureWasActiveInPreviousEnqueue = false;
};

AubSubCaptureStatus AubSubCaptureManager::checkAndActivateSubCapture(const MultiDispatchInfo &dispatchInfo) {
    if (dispatchInfo.empty()) {
        return {false, false};
    }

    auto guard = this->lock();

    kernelCurrentIdx = subCaptureCommon.getKernelCurrentIndexAndIncrement();

    subCaptureWasActiveInPreviousEnqueue = subCaptureIsActive;
    subCaptureIsActive = false;

    switch (subCaptureCommon.subCaptureMode) {
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

    return {subCaptureIsActive, subCaptureWasActiveInPreviousEnqueue};
}

const std::string &AubSubCaptureManager::getSubCaptureFileName(const MultiDispatchInfo &dispatchInfo) {
    auto guard = this->lock();

    if (useExternalFileName) {
        currentFileName = getExternalFileName();
    }
    switch (subCaptureCommon.subCaptureMode) {
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
    filterFileName += "_from_" + std::to_string(subCaptureCommon.subCaptureFilter.dumpKernelStartIdx);
    filterFileName += "_to_" + std::to_string(subCaptureCommon.subCaptureFilter.dumpKernelEndIdx);
    if (!subCaptureCommon.subCaptureFilter.dumpKernelName.empty()) {
        filterFileName += "_" + subCaptureCommon.subCaptureFilter.dumpKernelName;
        filterFileName += "_from_" + std::to_string(subCaptureCommon.subCaptureFilter.dumpNamedKernelStartIdx);
        filterFileName += "_to_" + std::to_string(subCaptureCommon.subCaptureFilter.dumpNamedKernelEndIdx);
    }
    filterFileName += ".aub";
    return filterFileName;
}

std::string AubSubCaptureManager::generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const {
    std::string baseFileName = initialFileName.substr(0, initialFileName.length() - strlen(".aub"));
    std::string toggleFileName = baseFileName + "_toggle";
    toggleFileName += "_from_" + std::to_string(kernelCurrentIdx);
    if (!dispatchInfo.empty()) {
        toggleFileName += "_" + dispatchInfo.peekMainKernel()->getKernelInfo().name;
    }
    toggleFileName += ".aub";
    return toggleFileName;
}

bool AubSubCaptureManager::isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo) {
    auto kernelName = dispatchInfo.peekMainKernel()->getKernelInfo().name;
    auto subCaptureIsActive = false;

    if (subCaptureCommon.subCaptureFilter.dumpKernelName.empty()) {
        if (isKernelIndexInSubCaptureRange(kernelCurrentIdx, subCaptureCommon.subCaptureFilter.dumpKernelStartIdx, subCaptureCommon.subCaptureFilter.dumpKernelEndIdx)) {
            subCaptureIsActive = true;
        }
    } else {
        if (0 == kernelName.compare(subCaptureCommon.subCaptureFilter.dumpKernelName)) {
            kernelNameMatchesNum = subCaptureCommon.getKernelNameMatchesNumAndIncrement();
            if (isKernelIndexInSubCaptureRange(kernelNameMatchesNum, subCaptureCommon.subCaptureFilter.dumpNamedKernelStartIdx, subCaptureCommon.subCaptureFilter.dumpNamedKernelEndIdx)) {
                subCaptureIsActive = true;
            }
        }
    }
    return subCaptureIsActive;
}

std::unique_lock<std::mutex> AubSubCaptureManager::lock() const {
    return std::unique_lock<std::mutex>{mutex};
}
} // namespace NEO
