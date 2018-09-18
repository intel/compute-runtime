/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
#include <string>

namespace OCLRT {

struct MultiDispatchInfo;
class SettingsReader;

class AubSubCaptureManager {
  public:
    enum class SubCaptureMode {
        Off = 0, //subcapture off
        Filter,  //subcapture kernel specified by filter (static regkey)
        Toggle   //toggle subcapture on/off (dynamic regkey)
    } subCaptureMode = SubCaptureMode::Off;

    struct SubCaptureFilter {
        std::string dumpKernelName = "";
        uint32_t dumpNamedKernelStartIdx = 0;
        uint32_t dumpNamedKernelEndIdx = static_cast<uint32_t>(-1);
        uint32_t dumpKernelStartIdx = 0;
        uint32_t dumpKernelEndIdx = static_cast<uint32_t>(-1);
    } subCaptureFilter;

    inline bool isSubCaptureMode() const {
        return subCaptureMode > SubCaptureMode::Off;
    }

    inline bool isSubCaptureEnabled() const {
        return subCaptureIsActive || subCaptureWasActive;
    }
    inline void disableSubCapture() {
        subCaptureIsActive = subCaptureWasActive = false;
    };

    bool activateSubCapture(const MultiDispatchInfo &dispatchInfo);

    const std::string &getSubCaptureFileName(const MultiDispatchInfo &dispatchInfo);

    AubSubCaptureManager(const std::string &fileName);
    virtual ~AubSubCaptureManager();

  protected:
    MOCKABLE_VIRTUAL bool isSubCaptureToggleActive() const;
    bool isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo);
    MOCKABLE_VIRTUAL std::string getExternalFileName() const;
    MOCKABLE_VIRTUAL std::string generateFilterFileName() const;
    MOCKABLE_VIRTUAL std::string generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const;
    bool isKernelIndexInSubCaptureRange(uint32_t kernelIdx, uint32_t rangeStartIdx, uint32_t rangeEndIdx) const;
    void setDebugManagerFlags() const;

    bool subCaptureIsActive = false;
    bool subCaptureWasActive = false;
    uint32_t kernelCurrentIdx = 0;
    uint32_t kernelNameMatchesNum = 0;
    bool useExternalFileName = true;
    std::string initialFileName;
    std::string currentFileName;
    std::unique_ptr<SettingsReader> settingsReader;
};
} // namespace OCLRT
