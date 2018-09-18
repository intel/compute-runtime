/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

namespace OCLRT {

class OsLibrary {
  protected:
    OsLibrary() = default;

  public:
    virtual ~OsLibrary() = default;

    static OsLibrary *load(const std::string &name);

    virtual void *getProcAddress(const std::string &procName) = 0;
    virtual bool isLoaded() = 0;
};
} // namespace OCLRT
