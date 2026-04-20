/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "helper.h"

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_inc_base.h"

#include "neo_igfxfmid.h"

#include <algorithm>
#include <fstream>

void (*abortOclocExecution)(int) = abortOclocExecutionDefaultHandler;

void abortOclocExecutionDefaultHandler(int errorCode) {
    exit(errorCode);
}

void addSlash(std::string &path) {
    if (!path.empty()) {
        auto lastChar = *path.rbegin();
        if ((lastChar != '/') && (lastChar != '\\')) {
            path.append("/");
        }
    }
}

std::vector<char> readBinaryFile(const std::string &fileName) {
    size_t length;
    std::unique_ptr<char[]> data = NEO::loadDataFromFile(fileName.c_str(), length);

    if (data) {
        std::vector<char> binary(data.get(), data.get() + length);
        return binary;
    } else {
        printf("Error! Couldn't open %s\n", fileName.c_str());
        abortOclocExecution(1);

        return {};
    }
}

void istreamToVectorOfStrings(std::istream &input, std::vector<std::string> &lines, bool replaceTabs) {
    if (input.good()) {
        if (replaceTabs) {
            for (std::string line; std::getline(input, line);) {
                std::replace_if(
                    line.begin(), line.end(), [](auto c) { return c == '\t'; }, ' ');
                lines.push_back(std::move(line));
            }
        } else {
            for (std::string line; std::getline(input, line);) {
                lines.push_back(std::move(line));
            }
        }
    }
}

void readFileToVectorOfStrings(std::vector<std::string> &lines, const std::string &fileName, bool replaceTabs) {
    size_t fileSize = 0;
    std::unique_ptr<char[]> fileData = NEO::loadDataFromFile(fileName.c_str(), fileSize);

    if (fileData && fileSize > 0) {
        std::istringstream inputStream(std::string(fileData.get(), fileSize));
        istreamToVectorOfStrings(inputStream, lines, replaceTabs);
    }
}

PRODUCT_FAMILY getProductFamilyFromDeviceName(const std::string &deviceName) {
    for (unsigned int productId = 0; productId < NEO::maxProductEnumValue; ++productId) {
        if (NEO::hardwarePrefix[productId] != nullptr &&
            deviceName == NEO::hardwarePrefix[productId]) {
            return static_cast<PRODUCT_FAMILY>(productId);
        }
    }
    return IGFX_UNKNOWN;
}
