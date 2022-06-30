/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/debug_settings_reader.h"

#include <memory>
#include <string>

namespace NEO {
class SettingsReaderCreator {
  public:
    static std::unique_ptr<SettingsReader> create(const std::string &regKey);
};
} // namespace NEO
