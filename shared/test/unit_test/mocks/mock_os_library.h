/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

class MockOsLibrary : public NEO::OsLibrary {
  public:
    void *getProcAddress(const std::string &procName) override {
        return nullptr;
    }
    bool isLoaded() override {
        return false;
    }
};
