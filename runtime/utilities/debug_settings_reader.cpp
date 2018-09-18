/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "debug_settings_reader.h"
#include "debug_file_reader.h"

namespace OCLRT {

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
}; // namespace OCLRT
