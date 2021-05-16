/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_settings_reader_creator.h"

namespace NEO {
std::unique_ptr<SettingsReader> SettingsReaderCreator::create(const std::string &regKey) {
    return std::unique_ptr<SettingsReader>(SettingsReader::create(regKey));
}
}; // namespace NEO
