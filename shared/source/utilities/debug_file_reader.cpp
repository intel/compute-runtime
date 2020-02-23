/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/debug_file_reader.h"

using namespace std;

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
    int32_t value = defaultValue;

    map<string, string>::iterator it = settingStringMap.find(string(settingName));
    if (it != settingStringMap.end()) {
        value = atoi(it->second.c_str());
    }

    return value;
}

bool SettingsFileReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int32_t>(defaultValue)) ? true : false;
}

std::string SettingsFileReader::getSetting(const char *settingName, const std::string &value) {
    std::string returnValue = value;
    map<string, string>::iterator it = settingStringMap.find(string(settingName));
    if (it != settingStringMap.end())
        returnValue = it->second;

    return returnValue;
}

const char *SettingsFileReader::appSpecificLocation(const std::string &name) {
    return name.c_str();
}

void SettingsFileReader::parseStream(std::istream &inputStream) {
    stringstream ss;
    string key;
    string value;
    char temp = 0;

    while (!inputStream.eof()) {
        string tempString;
        string tempStringValue;
        getline(inputStream, tempString);

        ss << tempString;
        ss >> key;
        ss >> temp;
        ss >> value;

        if (!ss.fail()) {
            settingStringMap.insert(pair<string, string>(key, value));
        }

        ss.str(string()); // for reset string inside stringstream
        ss.clear();
        key.clear();
    }
}
}; // namespace NEO
