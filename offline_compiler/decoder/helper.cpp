/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "helper.h"

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/os_inc_base.h"
#include "runtime/os_interface/os_library.h"

#include "igfxfmid.h"

#include <algorithm>
#include <fstream>

void addSlash(std::string &path) {
    if (!path.empty()) {
        auto lastChar = *path.rbegin();
        if ((lastChar != '/') && (lastChar != '\\')) {
            path += '/';
        }
    }
}

std::vector<char> readBinaryFile(const std::string &fileName) {
    std::ifstream file(fileName, std::ios_base::binary);
    if (file.good()) {
        size_t length;
        file.seekg(0, file.end);
        length = static_cast<size_t>(file.tellg());
        file.seekg(0, file.beg);
        std::vector<char> binary(length);
        file.read(binary.data(), length);
        return binary;
    } else {
        printf("Error! Couldn't open %s\n", fileName.c_str());
        exit(1);
    }
}

void readFileToVectorOfStrings(std::vector<std::string> &lines, const std::string &fileName, bool replaceTabs) {
    std::ifstream file(fileName);
    if (file.good()) {
        if (replaceTabs) {
            for (std::string line; std::getline(file, line);) {
                std::replace_if(
                    line.begin(), line.end(), [](auto c) { return c == '\t'; }, ' ');
                lines.push_back(std::move(line));
            }
        } else {
            for (std::string line; std::getline(file, line);) {
                lines.push_back(std::move(line));
            }
        }
    }
}

size_t findPos(const std::vector<std::string> &lines, const std::string &whatToFind) {
    for (size_t i = 0; i < lines.size(); ++i) {
        auto it = lines[i].find(whatToFind);
        if (it != std::string::npos) {
            if (it + whatToFind.size() == lines[i].size()) {
                return i;
            }
            char delimiter = lines[i][it + whatToFind.size()];
            if ((delimiter == ' ') || (delimiter = '\t') || (delimiter = '\n') || (delimiter = '\r')) {
                return i;
            }
        }
    }
    return lines.size();
}

PRODUCT_FAMILY getProductFamilyFromDeviceName(const std::string &deviceName) {
    for (unsigned int productId = 0; productId < IGFX_MAX_PRODUCT; ++productId) {
        if (NEO::hardwarePrefix[productId] != nullptr &&
            deviceName == NEO::hardwarePrefix[productId]) {
            return static_cast<PRODUCT_FAMILY>(productId);
        }
    }
    return IGFX_UNKNOWN;
}
