/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"

#include <cstring>
#include <memory>

namespace Ocloc {
using namespace NEO;

std::string SupportedDevicesHelper::getOutputFilenameSuffix(SupportedDevicesMode mode) const {
    return "_supported_devices_" + toStr(mode) + fileExtension.data();
}

std::string SupportedDevicesHelper::getCurrentOclocOutputFilename() const {
    return getCurrentOclocName() + getOutputFilenameSuffix(mode);
}

std::string SupportedDevicesHelper::getCurrentOclocName() const {
    return extractOclocName(getOclocCurrentLibName());
}

std::string SupportedDevicesHelper::extractOclocName(std::string_view oclocLibName) const {
    // libocloc.so -> ocloc
    // libocloc-legacy1.so -> ocloc-legacy1
    std::string_view view(oclocLibName);
    constexpr char prefix[] = "lib";
    auto start = view.find(prefix);
    if (start == std::string_view::npos) {
        return "ocloc";
    }
    start += strlen(prefix);

    auto end = view.find(".so", start);
    if (end == std::string_view::npos) {
        return "ocloc";
    }

    return std::string(view.substr(start, end - start));
}

std::string SupportedDevicesHelper::getDataFromFormerOcloc() const {
    std::string retData;

    const char *argv[] = {"ocloc", "query", "SUPPORTED_DEVICES", "-concat"};

    unsigned int numArgs = sizeof(argv) / sizeof(argv[0]);
    uint32_t numOutputs = 0u;
    unsigned char **dataOutputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    auto retVal = Commands::invokeFormerOcloc(getOclocFormerLibName(),
                                              numArgs, argv,
                                              0,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              0,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              &numOutputs,
                                              &dataOutputs,
                                              &ouputLengths,
                                              &outputNames);
    if (!retVal) {
        return "";
    }

    const std::string expectedSubstr = getOutputFilenameSuffix(SupportedDevicesMode::concat);

    for (unsigned int i = 0; i < numOutputs; ++i) {
        if (std::strstr(outputNames[i], expectedSubstr.c_str()) == nullptr) {
            continue;
        }
        retData = std::string(reinterpret_cast<char *>(dataOutputs[i]), ouputLengths[i]);
        break;
    }

    oclocFreeOutput(&numOutputs, &dataOutputs, &ouputLengths, &outputNames);
    return retData;
}

} // namespace Ocloc
