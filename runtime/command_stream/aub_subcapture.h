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
    bool isSubCaptureFilterActive(const MultiDispatchInfo &dispatchInfo, uint32_t kernelIdx) const;
    MOCKABLE_VIRTUAL std::string getExternalFileName() const;
    MOCKABLE_VIRTUAL std::string generateFilterFileName() const;
    MOCKABLE_VIRTUAL std::string generateToggleFileName(const MultiDispatchInfo &dispatchInfo) const;
    bool isKernelIndexInSubCaptureRange(uint32_t kernelIdx) const;
    void setDebugManagerFlags() const;

    bool subCaptureIsActive = false;
    bool subCaptureWasActive = false;
    uint32_t kernelCurrentIdx = 0;
    bool useExternalFileName = true;
    std::string initialFileName;
    std::string currentFileName;
    std::unique_ptr<SettingsReader> settingsReader;
};
} // namespace OCLRT
