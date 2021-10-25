/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/logger.h"

#include "opencl/source/utilities/cl_logger.h"

#include <map>

template <DebugFunctionalityLevel DebugLevel>
class TestFileLogger : public NEO::FileLogger<DebugLevel> {
  public:
    using NEO::FileLogger<DebugLevel>::FileLogger;

    ~TestFileLogger() {
        std::remove(NEO::FileLogger<DebugLevel>::logFileName.c_str());
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
            NEO::FileLogger<DebugLevel>::writeToFile(filename, str, length, mode);
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

using FullyEnabledFileLogger = TestFileLogger<DebugFunctionalityLevel::Full>;
using FullyDisabledFileLogger = TestFileLogger<DebugFunctionalityLevel::None>;

using FullyEnabledClFileLogger = NEO::ClFileLogger<DebugFunctionalityLevel::Full>;
using FullyDisabledClFileLogger = NEO::ClFileLogger<DebugFunctionalityLevel::None>;

template <bool DebugFunctionality>
class TestLoggerApiEnterWrapper : public NEO::LoggerApiEnterWrapper<DebugFunctionality> {
  public:
    TestLoggerApiEnterWrapper(const char *functionName, int *errCode) : NEO::LoggerApiEnterWrapper<DebugFunctionality>(functionName, errCode), loggedEnter(false) {
        if (DebugFunctionality) {
            loggedEnter = true;
        }
    }

    bool loggedEnter;
};
