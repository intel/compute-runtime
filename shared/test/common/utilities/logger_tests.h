/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/logger.h"

#include <map>

template <DebugFunctionalityLevel debugLevel>
class TestFileLogger : public NEO::FileLogger<debugLevel> {
  public:
    using NEO::FileLogger<debugLevel>::FileLogger;

    ~TestFileLogger() override {
        std::remove(NEO::FileLogger<debugLevel>::logFileName.c_str());
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
            NEO::FileLogger<debugLevel>::writeToFile(filename, str, length, mode);
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

using FullyEnabledFileLogger = TestFileLogger<DebugFunctionalityLevel::full>;
using FullyDisabledFileLogger = TestFileLogger<DebugFunctionalityLevel::none>;
using ReleaseInternalFileLogger = TestFileLogger<DebugFunctionalityLevel::regKeys>;