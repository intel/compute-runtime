/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_file_reader.h"


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
        value = atoi(it->second.c_str());
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
    std::stringstream ss;
    std::string key;
    std::string value;
    char temp = 0;

    while (!inputStream.eof()) {
        std::string tempString;
        std::string tempStringValue;
        getline(inputStream, tempString);

        ss << tempString;
        ss >> key;
        ss >> temp;
        ss >> value;

        if (!ss.fail()) {
            settingStringMap.insert(std::pair<std::string, std::string>(key, value));
        }

        ss.str(std::string()); // for reset string inside stringstream
        ss.clear();
        key.clear();
    }
}
}; // namespace NEO
