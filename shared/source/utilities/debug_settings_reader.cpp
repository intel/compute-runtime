/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_settings_reader.h"

#include "shared/source/utilities/debug_file_reader.h"

#include <fstream>
#include <vector>

namespace NEO {

const char *SettingsReader::settingsFileName = "igdrcl.config";
const char *SettingsReader::neoSettingsFileName = "neo.config";

SettingsReader *SettingsReader::createFileReader() {
    std::ifstream settingsFile;
    std::vector<const char *> fileName;
    fileName.push_back(settingsFileName);
    fileName.push_back(neoSettingsFileName);
    for (const auto &file : fileName) {
        settingsFile.open(file);
        if (settingsFile.is_open()) {
            settingsFile.close();
            return new SettingsFileReader(file);
        }
    }
    return nullptr;
}
}; // namespace NEO
