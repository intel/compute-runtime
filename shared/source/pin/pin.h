/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

namespace NEO {

class PinContext {
  public:
    static bool init(const std::string &gtPinOpenFunctionName);

  private:
    static const std::string gtPinLibraryFilename;
};

} // namespace NEO
