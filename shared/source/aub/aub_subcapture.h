/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/aub_subcapture_status.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace NEO {

class SettingsReader;

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

    AubSubCaptureStatus checkAndActivateSubCapture(const std::string &kernelName);

    AubSubCaptureStatus getSubCaptureStatus() const;

    const std::string &getSubCaptureFileName(const std::string &kernelName);

    AubSubCaptureManager(const std::string &fileName, AubSubCaptureCommon &subCaptureCommon, const char *regPath);
    virtual ~AubSubCaptureManager();

  protected:
    MOCKABLE_VIRTUAL bool isSubCaptureToggleActive() const;
    bool isSubCaptureFilterActive(const std::string &kernelName);
    MOCKABLE_VIRTUAL std::string getAubCaptureFileName() const;
    MOCKABLE_VIRTUAL std::string getToggleFileName() const;
    MOCKABLE_VIRTUAL std::string generateFilterFileName() const;
    MOCKABLE_VIRTUAL std::string generateToggleFileName(const std::string &kernelName) const;
    bool isKernelIndexInSubCaptureRange(uint32_t kernelIdx, uint32_t rangeStartIdx, uint32_t rangeEndIdx) const;
    MOCKABLE_VIRTUAL std::unique_lock<std::mutex> lock() const;

    bool subCaptureIsActive = false;
    bool subCaptureWasActiveInPreviousEnqueue = false;
    uint32_t kernelCurrentIdx = 0;
    uint32_t kernelNameMatchesNum = 0;
    bool useToggleFileName = true;
    std::string initialFileName;
    std::string currentFileName;
    std::unique_ptr<SettingsReader> settingsReader;
    AubSubCaptureCommon &subCaptureCommon;
    mutable std::mutex mutex;
};
} // namespace NEO
