/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include <unordered_map>
class MockOsLibrary : public NEO::OsLibrary {
  public:
    MockOsLibrary(void *procAddress, bool isLoaded) : getProcAddressReturn{procAddress}, isLoadedReturn{isLoaded} {}
    MockOsLibrary() {}

    std::string lastRequestedProcName;
    void *getProcAddressReturn = nullptr;

    void *getProcAddress(const std::string &procName) override {
        lastRequestedProcName = procName;
        return getProcAddressReturn;
    }

    bool isLoadedReturn = false;

    bool isLoaded() override {
        return isLoadedReturn;
    }

    std::string getFullPath() override {
        return std::string();
    }

    static OsLibrary *loadLibraryNewObject;

    static OsLibrary *load(const NEO::OsLibraryCreateProperties &properties) {
        if (properties.errorValue) {
            return OsLibrary::load(properties);
        }
        OsLibrary *ptr = loadLibraryNewObject;
        loadLibraryNewObject = nullptr;
        return ptr;
    }
};

class MockOsLibraryCustom : public MockOsLibrary {
  public:
    using MockOsLibrary::MockOsLibrary;
    std::unordered_map<std::string, void *> procMap;
    void *getProcAddress(const std::string &procName) override {
        if (procMap.find(procName) != procMap.end()) {
            return procMap[procName];
        } else {
            return getProcAddressReturn;
        }
    }
};
