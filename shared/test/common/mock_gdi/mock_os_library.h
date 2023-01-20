/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

namespace NEO {

class MockOsLibrary : public NEO::OsLibrary {
  public:
    MockOsLibrary(){};
    void *getProcAddress(const std::string &procName) override;
    bool isLoaded() override;

    static OsLibrary *load(const std::string &name) {
        return new MockOsLibrary();
    }
};

} // namespace NEO
