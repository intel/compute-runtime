/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/helpers/mock_file_io.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace NEO {
class MockSettingsFileReader : public SettingsFileReader {
  public:
    MockSettingsFileReader(const char *filePath) : SettingsFileReader("") {
        UNRECOVERABLE_IF(!filePath);
        UNRECOVERABLE_IF(!virtualFileExists(filePath));

        if (virtualFileExists(filePath)) {
            size_t fileSize = 0;
            std::unique_ptr<char[]> fileData = loadDataFromVirtualFile(filePath, fileSize);

            if (fileData && fileSize > 0) {
                std::istringstream settingsFile(std::string(fileData.get(), fileSize));
                parseStream(settingsFile);
            }
        }
    }
};

class MockSettingsReader : public SettingsReader {
  public:
    static SettingsReader *createFileReader() {
        std::vector<const char *> fileName;
        fileName.push_back(settingsFileName);
        fileName.push_back(neoSettingsFileName);
        for (const auto &file : fileName) {
            if (virtualFileExists(file)) {
                return new MockSettingsFileReader(file);
            }
        }
        return nullptr;
    }

    static SettingsReader *create(const std::string &regKey) {
        SettingsReader *readerImpl = MockSettingsReader::createFileReader();
        if (readerImpl != nullptr)
            return readerImpl;

        return createOsReader(false, regKey);
    }
};
} // namespace NEO