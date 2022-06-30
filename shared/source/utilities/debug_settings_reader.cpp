/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_settings_reader.h"

#include "shared/source/utilities/debug_file_reader.h"

#include <fstream>

namespace NEO {

const char *SettingsReader::settingsFileName = "igdrcl.config";

SettingsReader *SettingsReader::createFileReader() {
    std::ifstream settingsFile;
    settingsFile.open(settingsFileName);
    if (settingsFile.is_open()) {
        settingsFile.close();
        return new SettingsFileReader();
    }
    return nullptr;
}
}; // namespace NEO
