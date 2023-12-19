/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_subcapture.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/debug_settings_reader.h"

namespace NEO {

AubSubCaptureManager::AubSubCaptureManager(const std::string &fileName, AubSubCaptureCommon &subCaptureCommon, const char *regPath)
    : initialFileName(fileName), subCaptureCommon(subCaptureCommon) {
    settingsReader.reset(SettingsReader::createOsReader(true, regPath));
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

AubSubCaptureStatus AubSubCaptureManager::checkAndActivateSubCapture(const std::string &kernelName) {
    if (kernelName.empty()) {
        return {false, false};
    }

    auto guard = this->lock();

    kernelCurrentIdx = subCaptureCommon.getKernelCurrentIndexAndIncrement();

    subCaptureWasActiveInPreviousEnqueue = subCaptureIsActive;
    subCaptureIsActive = false;

    switch (subCaptureCommon.subCaptureMode) {
    case SubCaptureMode::toggle:
        subCaptureIsActive = isSubCaptureToggleActive();
        break;
    case SubCaptureMode::filter:
        subCaptureIsActive = isSubCaptureFilterActive(kernelName);
        break;
    default:
        DEBUG_BREAK_IF(false);
        break;
    }

    return {subCaptureIsActive, subCaptureWasActiveInPreviousEnqueue};
}

AubSubCaptureStatus AubSubCaptureManager::getSubCaptureStatus() const {
    auto guard = this->lock();

    return {this->subCaptureIsActive, this->subCaptureWasActiveInPreviousEnqueue};
}

const std::string &AubSubCaptureManager::getSubCaptureFileName(const std::string &kernelName) {
    auto guard = this->lock();

    if (useToggleFileName) {
        currentFileName = getToggleFileName();
    }

    if (currentFileName.empty()) {
        currentFileName = getAubCaptureFileName();
        useToggleFileName = false;
    }

    switch (subCaptureCommon.subCaptureMode) {
    case SubCaptureMode::filter:
        if (currentFileName.empty()) {
            currentFileName = generateFilterFileName();
            useToggleFileName = false;
        }
        break;
    case SubCaptureMode::toggle:
        if (currentFileName.empty()) {
            currentFileName = generateToggleFileName(kernelName);
            useToggleFileName = false;
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

std::string AubSubCaptureManager::getToggleFileName() const {
    return settingsReader->getSetting("AUBDumpToggleFileName", std::string(""));
}

std::string AubSubCaptureManager::getAubCaptureFileName() const {
    if (debugManager.flags.AUBDumpCaptureFileName.get() != "unk") {
        return debugManager.flags.AUBDumpCaptureFileName.get();
    }
    return {};
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

std::string AubSubCaptureManager::generateToggleFileName(const std::string &kernelName) const {
    std::string baseFileName = initialFileName.substr(0, initialFileName.length() - strlen(".aub"));
    std::string toggleFileName = baseFileName + "_toggle";
    toggleFileName += "_from_" + std::to_string(kernelCurrentIdx);
    if (!kernelName.empty()) {
        toggleFileName += "_" + kernelName;
    }
    toggleFileName += ".aub";
    return toggleFileName;
}

bool AubSubCaptureManager::isSubCaptureFilterActive(const std::string &kernelName) {
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
