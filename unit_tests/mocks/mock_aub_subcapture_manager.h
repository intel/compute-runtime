/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_subcapture.h"

using namespace NEO;

class AubSubCaptureManagerMock : public AubSubCaptureManager {
  public:
    using AubSubCaptureManager::AubSubCaptureManager;

    void setSubCaptureIsActive(bool on) {
        subCaptureIsActive = on;
    }
    bool isSubCaptureActive() const {
        return subCaptureIsActive;
    }
    void setSubCaptureWasActiveInPreviousEnqueue(bool on) {
        subCaptureWasActiveInPreviousEnqueue = on;
    }
    bool wasSubCaptureActiveInPreviousEnqueue() const {
        return subCaptureWasActiveInPreviousEnqueue;
    }
    void setKernelCurrentIndex(uint32_t index) {
        kernelCurrentIdx = index;
    }
    uint32_t getKernelCurrentIndex() const {
        return kernelCurrentIdx;
    }
    bool getUseExternalFileName() const {
        return useExternalFileName;
    }
    const std::string &getInitialFileName() const {
        return initialFileName;
    }
    const std::string &getCurrentFileName() const {
        return currentFileName;
    }
    SettingsReader *getSettingsReader() const {
        return settingsReader.get();
    }
    void setSubCaptureToggleActive(bool on) {
        isToggledOn = on;
    }
    bool isSubCaptureToggleActive() const override {
        return isToggledOn;
    }
    void setExternalFileName(const std::string &fileName) {
        externalFileName = fileName;
    }
    std::string getExternalFileName() const override {
        return externalFileName;
    }

    std::unique_lock<std::mutex> lock() const override {
        isLocked = true;
        return std::unique_lock<std::mutex>{mutex};
    }

    using AubSubCaptureManager::generateFilterFileName;
    using AubSubCaptureManager::generateToggleFileName;

    mutable bool isLocked = false;

  protected:
    bool isToggledOn = false;
    std::string externalFileName = "";
};
