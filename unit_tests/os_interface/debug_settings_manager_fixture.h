/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/utilities/directory.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/string_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include <map>

using namespace NEO;
using namespace std;

#undef DECLARE_DEBUG_VARIABLE

class SettingsFileCreator {
  public:
    SettingsFileCreator(string &content) {
        bool settingsFileExists = fileExists(fileName);
        if (settingsFileExists) {
            remove(fileName.c_str());
        }

        writeDataToFile(fileName.c_str(), content.c_str(), content.size());
    };

    ~SettingsFileCreator() {
        remove(fileName.c_str());
    };

  private:
    string fileName = "igdrcl.config";
};

class TestDebugFlagsChecker {
  public:
    static bool isEqual(int32_t returnedValue, bool defaultValue) {
        if (returnedValue == 0) {
            return !defaultValue;
        } else {
            return defaultValue;
        }
    }

    static bool isEqual(int32_t returnedValue, int32_t defaultValue) {
        return returnedValue == defaultValue;
    }

    static bool isEqual(string returnedValue, string defaultValue) {
        return returnedValue == defaultValue;
    }
};

template <DebugFunctionalityLevel DebugLevel>
class TestDebugSettingsManager : public DebugSettingsManager<DebugLevel> {
  public:
    using DebugSettingsManager<DebugLevel>::dumpFlags;
    using DebugSettingsManager<DebugLevel>::settingsDumpFileName;

    ~TestDebugSettingsManager() {
        remove(DebugSettingsManager<DebugLevel>::logFileName.c_str());
    }
    SettingsReader *getSettingsReader() {
        return DebugSettingsManager<DebugLevel>::readerImpl.get();
    }

    void useRealFiles(bool value) {
        mockFileSystem = !value;
    }

    void writeToFile(std::string filename,
                     const char *str,
                     size_t length,
                     std::ios_base::openmode mode) override {

        savedFiles[filename] << std::string(str, str + length);
        if (mockFileSystem == false) {
            DebugSettingsManager<DebugLevel>::writeToFile(filename, str, length, mode);
        }
    };

    int32_t createdFilesCount() {
        return static_cast<int32_t>(savedFiles.size());
    }

    bool wasFileCreated(std::string filename) {
        return savedFiles.find(filename) != savedFiles.end();
    }

    std::string getFileString(std::string filename) {
        return savedFiles[filename].str();
    }

  protected:
    bool mockFileSystem = true;
    std::map<std::string, std::stringstream> savedFiles;
};

template <bool DebugFunctionality>
class TestDebugSettingsApiEnterWrapper : public DebugSettingsApiEnterWrapper<DebugFunctionality> {
  public:
    TestDebugSettingsApiEnterWrapper(const char *functionName, int *errCode) : DebugSettingsApiEnterWrapper<DebugFunctionality>(functionName, errCode), loggedEnter(false) {
        if (DebugFunctionality) {
            loggedEnter = true;
        }
    }

    bool loggedEnter;
};

using FullyEnabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::Full>;
using FullyDisabledTestDebugManager = TestDebugSettingsManager<DebugFunctionalityLevel::None>;
