/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/string.h"

#include <algorithm>
#include <map>
#include <string>

class MockOclocArgHelper : public OclocArgHelper {
  public:
    using FileName = std::string;
    using FileData = std::string;
    using FilesMap = std::map<FileName, FileData>;
    using OclocArgHelper::deviceProductTable;
    FilesMap &filesMap;
    bool interceptOutput{false};
    bool shouldReturnReadingError{false};
    FilesMap interceptedFiles;

    MockOclocArgHelper(FilesMap &filesMap) : OclocArgHelper(
                                                 0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
                                             filesMap(filesMap){};

  protected:
    bool fileExists(const std::string &filename) const override {
        return filesMap.find(filename) != filesMap.end();
    }

    std::vector<char> readBinaryFile(const std::string &filename) override {
        auto file = filesMap[filename];
        return std::vector<char>(file.begin(), file.end());
    }

    std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize) override {
        if (shouldReturnReadingError) {
            return nullptr;
        }

        if (!fileExists(filename)) {
            return OclocArgHelper::loadDataFromFile(filename, retSize);
        }

        const auto &file = filesMap[filename];
        std::unique_ptr<char[]> result{new char[file.size()]};

        std::copy(file.begin(), file.end(), result.get());
        retSize = file.size();

        return result;
    }

    void saveOutput(const std::string &filename, const void *pData, const size_t &dataSize) override {
        if (interceptOutput) {
            auto &fileContent = interceptedFiles[filename];
            fileContent.resize(dataSize, '\0');

            memcpy_s(fileContent.data(), fileContent.size(), pData, dataSize);
        } else {
            OclocArgHelper::saveOutput(filename, pData, dataSize);
        }
    }
};
