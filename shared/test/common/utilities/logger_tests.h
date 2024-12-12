/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string_helpers.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/logger.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include <map>

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
} // namespace NEO

template <DebugFunctionalityLevel debugLevel>
class TestFileLogger : public NEO::FileLogger<debugLevel> {
  public:
    using NEO::FileLogger<debugLevel>::FileLogger;

    TestFileLogger(std::string filename, const NEO::DebugVariables &flags) : NEO::FileLogger<debugLevel>(filename, flags) {
        if (NEO::FileLogger<debugLevel>::enabled() && virtualFileExists(this->getLogFileName())) {
            removeVirtualFile(this->getLogFileName());
        }
    }

    ~TestFileLogger() override {
        if (virtualFileExists(this->getLogFileName())) {
            removeVirtualFile(this->getLogFileName());
        }
    }

    void writeToFile(std::string filename,
                     const char *str,
                     size_t length,
                     std::ios_base::openmode mode) override {

        NEO::FileLogger<debugLevel>::writeToFile(filename, str, length, mode);
    }

    int32_t createdFilesCount() {
        return static_cast<int32_t>(NEO::virtualFileList.size());
    }

    bool wasFileCreated(std::string filename) {
        return virtualFileExists(filename);
    }

    std::string getFileString(std::string filename) {
        return NEO::virtualFileList[filename].str();
    }
};

using FullyEnabledFileLogger = TestFileLogger<DebugFunctionalityLevel::full>;
using FullyDisabledFileLogger = TestFileLogger<DebugFunctionalityLevel::none>;
using ReleaseInternalFileLogger = TestFileLogger<DebugFunctionalityLevel::regKeys>;
