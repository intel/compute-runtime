/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_settings_reader_creator.h"

namespace NEO {

std::unique_ptr<SettingsReader> mockSettingsReader;

std::unique_ptr<SettingsReader> SettingsReaderCreator::create(const std::string &regKey) {
    if (mockSettingsReader) {
        return std::move(mockSettingsReader);
    }
    return std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, regKey));
}
} // namespace NEO
