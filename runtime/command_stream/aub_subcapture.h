/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace NEO {

struct MultiDispatchInfo;
class SettingsReader;

struct AubSubCaptureStatus {
    bool isActive;
    bool wasActiveInPreviousEnqueue;
};

class AubSubCaptureCommon {
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

    inline uint32_t getKernelCurrentIndexAndIncrement() { return kernelCurrentIdx.fetch_add(1); }
    inline uint32_t getKernelNameMatchesNumAndIncrement() { return kernelNameMatchesNum.fetch_add(1); }

  protected:
    std::atomic<uint32_t> kernelCurrentIdx{0};
    std::atomic<uint32_t> kernelNameMatchesNum{0};
};

class AubSubCaptureManager {
  public:
    using SubCaptureMode = AubSubCaptureCommon::SubCaptureMode;
    using SubCaptureFilter = AubSubCaptureCommon::SubCaptureFilter;

    inline bool isSubCaptureMode() const {
        return subCaptureCommon.subCaptureMode > SubCaptureMode::Off;
    }

    bool isSubCaptureEnabled() const;

    void disableSubCapture();

    AubSubCaptureStatus checkAndActivateSubCapture(const MultiDispatchInfo &dispatchInfo);

    const std::string &getSubCaptureFileName(const MultiDispatchInfo &dispatchInfo);

    AubSubCaptureManager(const std::string &fileName, AubSubCaptureCommon &subCaptureCommon);
    virtual ~AubSubCaptureManager();

  protected:
    MOCKABLE_VIRTUAL bool isSubCaptureToggleActive() const;
    bool isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo);
    MOCKABLE_VIRTUAL std::string getExternalFileName() const;
    MOCKABLE_VIRTUAL std::string generateFilterFileName() const;
    MOCKABLE_VIRTUAL std::string generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const;
    bool isKernelIndexInSubCaptureRange(uint32_t kernelIdx, uint32_t rangeStartIdx, uint32_t rangeEndIdx) const;
    MOCKABLE_VIRTUAL std::unique_lock<std::mutex> lock() const;

    bool subCaptureIsActive = false;
    bool subCaptureWasActiveInPreviousEnqueue = false;
    uint32_t kernelCurrentIdx = 0;
    uint32_t kernelNameMatchesNum = 0;
    bool useExternalFileName = true;
    std::string initialFileName;
    std::string currentFileName;
    std::unique_ptr<SettingsReader> settingsReader;
    AubSubCaptureCommon &subCaptureCommon;
    mutable std::mutex mutex;
};
} // namespace NEO
