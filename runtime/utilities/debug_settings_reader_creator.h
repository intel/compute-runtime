/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "debug_settings_reader.h"

#include <memory>

namespace OCLRT {
class SettingsReaderCreator {
  public:
    static std::unique_ptr<SettingsReader> create();
};
} // namespace OCLRT
