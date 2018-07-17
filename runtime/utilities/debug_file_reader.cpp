/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/utilities/debug_file_reader.h"

using namespace std;

namespace OCLRT {

SettingsFileReader::SettingsFileReader(const char *filePath) {
    std::ifstream settingsFile;

    if (filePath == nullptr)
        settingsFile.open(settingsFileName);
    else
        settingsFile.open(filePath);

    if (settingsFile.is_open()) {

        stringstream ss;
        string key;
        int32_t value = 0;
        char temp = 0;

        while (!settingsFile.eof()) {
            string tempString;
            string tempStringValue;
            getline(settingsFile, tempString);

            ss << tempString;
            ss >> key;
            ss >> temp;
            ss >> value;
            if (!ss.fail()) {
                settingValueMap.insert(pair<string, int32_t>(key, value));
            } else {
                stringstream ss2;
                ss2 << tempString;
                ss2 >> key;
                ss2 >> temp;
                ss2 >> tempStringValue;
                if (!ss2.fail())
                    settingStringMap.insert(pair<string, string>(key, tempStringValue));
            }

            ss.str(string()); // for reset string inside stringstream
            ss.clear();
            key.clear();
        }

        settingsFile.close();
    }
}

SettingsFileReader::~SettingsFileReader() {
    settingValueMap.clear();
    settingStringMap.clear();
}

int32_t SettingsFileReader::getSetting(const char *settingName, int32_t defaultValue) {
    int32_t value = defaultValue;
    map<string, int32_t>::iterator it = settingValueMap.find(string(settingName));
    if (it != settingValueMap.end())
        value = it->second;

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
}; // namespace OCLRT
