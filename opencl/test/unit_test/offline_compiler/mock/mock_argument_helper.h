/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/string.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>

class MockOclocArgHelper : public OclocArgHelper {
  public:
    using OclocArgHelper::hasOutput;
    using OclocArgHelper::headers;
    using OclocArgHelper::inputs;
    using OclocArgHelper::messagePrinter;

    using OclocArgHelper::findSourceFile;

    using FileName = std::string;
    using FileData = std::string;
    using FilesMap = std::map<FileName, FileData>;

    FilesMap &filesMap;
    bool interceptOutput{false};
    bool shouldLoadDataFromFileReturnZeroSize{false};
    FilesMap interceptedFiles;
    bool callBaseFileExists = false;
    bool callBaseReadBinaryFile = false;
    bool callBaseLoadDataFromFile = false;
    bool callBaseReadFileToVectorOfStrings = false;
    bool shouldReturnEmptyVectorOfStrings = false;

    MockOclocArgHelper(FilesMap &filesMap) : OclocArgHelper(0, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
                                             filesMap(filesMap){};

    void setAllCallBase(bool value) {
        callBaseFileExists = value;
        callBaseReadBinaryFile = value;
        callBaseLoadDataFromFile = value;
        callBaseReadFileToVectorOfStrings = value;
    }

    void readFileToVectorOfStrings(const std::string &filename, std::vector<std::string> &lines) override {

        if (shouldReturnEmptyVectorOfStrings) {
            lines.clear();
            return;
        } else if (filesMap.find(filename) != filesMap.end()) {
            auto mockInputFile = std::istringstream(filesMap[filename]);
            ::istreamToVectorOfStrings(mockInputFile, lines);
            return;
        }
        OclocArgHelper::readFileToVectorOfStrings(filename, lines);
    }

    std::vector<char> readBinaryFile(const std::string &filename) override {
        if (callBaseReadBinaryFile) {
            return OclocArgHelper::readBinaryFile(filename);
        }
        auto file = filesMap[filename];
        return std::vector<char>(file.begin(), file.end());
    }

    std::string getLog() {
        return messagePrinter.getLog().str();
    }

  protected:
    bool fileExists(const std::string &filename) const override {
        if (filesMap.find(filename) != filesMap.end()) {
            return true;
        }

        return OclocArgHelper::sourceFileExists(filename);
    }

    std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize) override {
        if (callBaseLoadDataFromFile) {
            return OclocArgHelper::loadDataFromFile(filename, retSize);
        }

        if (Source *s = findSourceFile(filename)) {
            auto size = s->length;
            std::unique_ptr<char[]> ret(new char[size]());
            memcpy_s(ret.get(), size, s->data, s->length);
            retSize = s->length;
            return ret;
        }

        if (shouldLoadDataFromFileReturnZeroSize) {
            retSize = 0;
            return {};
        }

        if (!fileExists(filename)) {
            return OclocArgHelper::loadDataFromFile(filename, retSize);
        } else {
            const auto &file = filesMap[filename];
            std::unique_ptr<char[]> result{new char[file.size() + 1]};
            std::copy(file.begin(), file.end(), result.get());
            result[file.size()] = '\0';
            retSize = file.size() + 1;

            return result;
        }
    }

    void saveOutput(const std::string &filename, const void *pData, const size_t &dataSize) override {
        filesMap[filename] = std::string(reinterpret_cast<const char *>(pData), dataSize);
        if (interceptOutput) {
            auto &fileContent = interceptedFiles[filename];
            fileContent.resize(dataSize, '\0');

            memcpy_s(fileContent.data(), fileContent.size(), pData, dataSize);
        } else {
            OclocArgHelper::saveOutput(filename, pData, dataSize);
        }
    }
};