/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

namespace NEO {
namespace Linux {

class OsLibrary : public NEO::OsLibrary {
  private:
    void *handle;

  public:
    OsLibrary(const std::string &name);
    ~OsLibrary() override;

    bool isLoaded() override;
    void *getProcAddress(const std::string &procName) override;
};
} // namespace Linux
} // namespace NEO
