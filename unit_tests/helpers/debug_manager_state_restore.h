/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) shrink(DebugManager.flags.variableName.getRef());
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
    }
    DebugVariables debugVarSnapshot;
    void *injectFcnSnapshot = nullptr;

  protected:
    void shrink(std::string &flag) {
        flag.shrink_to_fit();
    }
    void shrink(int32_t &flag) {}
    void shrink(bool &flag) {}
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
    const char *appSpecificLocation(const std::string &name) override {
        return name.c_str();
    }
};
