/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/debug_settings_reader_creator.h"

namespace OCLRT {
std::unique_ptr<SettingsReader> SettingsReaderCreator::create() {
    return std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false));
}
} // namespace OCLRT
