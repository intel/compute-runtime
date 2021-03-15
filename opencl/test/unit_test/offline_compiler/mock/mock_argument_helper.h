/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_arg_helper.h"

#include <map>
#include <string>

class MockOclocArgHelper : public OclocArgHelper {
  public:
    using FileName = std::string;
    using FileData = std::string;
    using FilesMap = std::map<FileName, FileData>;
    using OclocArgHelper::deviceProductTable;
    FilesMap &filesMap;
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
};
