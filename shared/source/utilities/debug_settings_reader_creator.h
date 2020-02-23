/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/debug_settings_reader.h"

#include <memory>

namespace NEO {
class SettingsReaderCreator {
  public:
    static std::unique_ptr<SettingsReader> create(const std::string &regKey);
};
} // namespace NEO
