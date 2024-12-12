/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "helper.h"

#include "shared/offline_compiler/source/decoder/iga_wrapper.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/os_inc_base.h"

#include "igfxfmid.h"

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
    std::ifstream file(fileName);
    istreamToVectorOfStrings(file, lines, replaceTabs);
}

size_t findPos(const std::vector<std::string> &lines, const std::string &whatToFind) {
    for (size_t i = 0; i < lines.size(); ++i) {
        auto it = lines[i].find(whatToFind);
        if (it != std::string::npos) {
            if (it + whatToFind.size() == lines[i].size()) {
                return i;
            }
            char delimiter = lines[i][it + whatToFind.size()];
            if ((' ' == delimiter) || ('\t' == delimiter) || ('\n' == delimiter) || ('\r' == delimiter)) {
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

void setProductFamilyForIga(const std::string &device, IgaWrapper *iga, OclocArgHelper *argHelper) {
    auto productFamily = argHelper->productConfigHelper->getProductFamilyFromDeviceName(device);
    if (productFamily == IGFX_UNKNOWN) {
        productFamily = getProductFamilyFromDeviceName(device);
        if (productFamily != IGFX_UNKNOWN) {
            argHelper->printf("Warning : Deprecated device name is being used.\n");
        }
    }
    iga->setProductFamily(productFamily);
}
