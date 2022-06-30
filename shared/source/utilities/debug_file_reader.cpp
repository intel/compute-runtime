/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_file_reader.h"

#include <fstream>
#include <sstream>

namespace NEO {

SettingsFileReader::SettingsFileReader(const char *filePath) {
    std::ifstream settingsFile;

    if (filePath == nullptr)
        settingsFile.open(settingsFileName);
    else
        settingsFile.open(filePath);

    if (settingsFile.is_open()) {
        parseStream(settingsFile);
        settingsFile.close();
    }
}

SettingsFileReader::~SettingsFileReader() {
    settingStringMap.clear();
}

int32_t SettingsFileReader::getSetting(const char *settingName, int32_t defaultValue) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue)));
}

int64_t SettingsFileReader::getSetting(const char *settingName, int64_t defaultValue) {
    int64_t value = defaultValue;

    std::map<std::string, std::string>::iterator it = settingStringMap.find(std::string(settingName));
    if (it != settingStringMap.end()) {
        value = strtoll(it->second.c_str(), nullptr, 0);
    }

    return value;
}

bool SettingsFileReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int64_t>(defaultValue)) ? true : false;
}

std::string SettingsFileReader::getSetting(const char *settingName, const std::string &value) {
    std::string returnValue = value;
    std::map<std::string, std::string>::iterator it = settingStringMap.find(std::string(settingName));
    if (it != settingStringMap.end())
        returnValue = it->second;

    return returnValue;
}

const char *SettingsFileReader::appSpecificLocation(const std::string &name) {
    return name.c_str();
}

void SettingsFileReader::parseStream(std::istream &inputStream) {
    std::string key;
    std::string value;
    std::string line;
    std::string tmp;

    while (!inputStream.eof()) {
        getline(inputStream, line);

        auto equalsSignPosition = line.find('=');
        if (equalsSignPosition == std::string::npos) {
            continue;
        }

        {
            std::stringstream ss;
            auto linePartWithKey = line.substr(0, equalsSignPosition);
            ss << linePartWithKey;
            ss >> key;
            if (ss.fail() || (ss >> tmp)) {
                continue;
            }
        }
        {
            std::stringstream ss;
            auto linePartWithValue = line.substr(equalsSignPosition + 1);
            ss << linePartWithValue;
            ss >> value;
            if (ss.fail() || (ss >> tmp)) {
                continue;
            }
        }

        settingStringMap.insert(std::pair<std::string, std::string>(key, value));
    }
}
}; // namespace NEO
