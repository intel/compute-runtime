/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/debug_settings_reader.h"

using namespace OCLRT;

class DebugManagerStateRestore {
  public:
    DebugManagerStateRestore() {
        debugVarSnapshot = DebugManager.flags;
        injectFcnSnapshot = DebugManager.injectFcn;
    }
    ~DebugManagerStateRestore() {
        DebugManager.flags = debugVarSnapshot;
        DebugManager.injectFcn = injectFcnSnapshot;
    }
    DebugSettingsManager<globalDebugFunctionalityLevel>::DebugVariables debugVarSnapshot;
    void *injectFcnSnapshot = nullptr;
};

class RegistryReaderMock : public SettingsReader {
  public:
    RegistryReaderMock() {}
    ~RegistryReaderMock() {}

    unsigned int forceRetValue = 1;

    int32_t getSetting(const char *settingName, int32_t defaultValue) override {
        return static_cast<int32_t>(forceRetValue);
    }
    bool getSetting(const char *settingName, bool defaultValue) override {
        return true;
    }
    std::string getSetting(const char *settingName, const std::string &value) override {
        return "";
    }
};
