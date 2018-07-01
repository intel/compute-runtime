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
#include "runtime/command_stream/aub_subcapture.h"

using namespace OCLRT;

class AubSubCaptureManagerMock : public AubSubCaptureManager {
  public:
    void setSubCaptureIsActive(bool on) {
        subCaptureIsActive = on;
    }
    bool isSubCaptureActive() const {
        return subCaptureIsActive;
    }
    void setSubCaptureWasActive(bool on) {
        subCaptureWasActive = on;
    }
    bool wasSubCaptureActive() const {
        return subCaptureIsActive;
    }
    void setKernelCurrentIndex(uint32_t index) {
        kernelCurrentIdx = index;
    }
    uint32_t getKernelCurrentIndex() const {
        return kernelCurrentIdx;
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

  protected:
    bool isToggledOn = false;
};
