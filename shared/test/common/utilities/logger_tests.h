/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string_helpers.h"
#include "shared/source/utilities/allocation_logger.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/logger.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include <map>
#include <utility>

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
} // namespace NEO

template <DebugFunctionalityLevel debugLevel>
class TestFileLogger : public NEO::FileLogger<debugLevel> {
  public:
    TestFileLogger(std::string filename) : NEO::FileLogger<debugLevel>(std::move(filename)) {
        if (NEO::FileLogger<debugLevel>::enabled() && virtualFileExists(this->getLogFileName())) {
            removeVirtualFile(this->getLogFileName());
        }
    }

    ~TestFileLogger() override {
        if (virtualFileExists(this->getLogFileName())) {
            removeVirtualFile(this->getLogFileName());
        }
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

template <DebugFunctionalityLevel debugLevel>
class TestAllocationLogger : public TestFileLogger<debugLevel>, public NEO::AllocationLogger<debugLevel> {
  public:
    TestAllocationLogger(std::string filename, const NEO::DebugVariables &flags)
        : TestFileLogger<debugLevel>(std::move(filename)), NEO::AllocationLogger<debugLevel>(*this, flags) {}
};

using FullyEnabledFileLogger = TestFileLogger<DebugFunctionalityLevel::full>;
using FullyDisabledFileLogger = TestFileLogger<DebugFunctionalityLevel::none>;
using ReleaseInternalFileLogger = TestFileLogger<DebugFunctionalityLevel::regKeys>;

using FullyEnabledAllocationLogger = TestAllocationLogger<DebugFunctionalityLevel::full>;
