/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

namespace NEO {

class PinContext {
  public:
    static bool init(const std::string &gtPinOpenFunctionName);
    using OsLibraryLoadPtr = std::add_pointer<NEO::OsLibrary *(const std::string &)>::type;
    static OsLibraryLoadPtr osLibraryLoadFunction;

  private:
    static const std::string gtPinLibraryFilename;
};

} // namespace NEO
