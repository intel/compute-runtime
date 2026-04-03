/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_settings_reader.h"

#include "shared/source/utilities/debug_file_reader.h"
#include "shared/source/utilities/io_functions.h"

#include <vector>

namespace NEO {

const char *SettingsReader::settingsFileName = "igdrcl.config";
const char *SettingsReader::neoSettingsFileName = "neo.config";

SettingsReader *SettingsReader::createFileReader() {
    std::vector<const char *> fileName;
    fileName.push_back(settingsFileName);
    fileName.push_back(neoSettingsFileName);
    for (const auto &file : fileName) {
        auto fp = IoFunctions::fopenPtr(file, "r");
        if (fp) {
            IoFunctions::fclosePtr(fp);
            return new SettingsFileReader(file);
        }
    }
    return nullptr;
}
}; // namespace NEO
