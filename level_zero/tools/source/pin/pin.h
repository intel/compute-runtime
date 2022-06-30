/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

namespace L0 {

class PinContext {
  public:
    static ze_result_t init();

  private:
    static const std::string gtPinLibraryFilename;
};

} // namespace L0
