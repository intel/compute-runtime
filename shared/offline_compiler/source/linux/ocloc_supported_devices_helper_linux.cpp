/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/source/os_interface/os_library.h"

#include <cstring>
#include <memory>

namespace Ocloc {
using namespace NEO;

std::string SupportedDevicesHelper::getOutputFilenameSuffix(SupportedDevicesMode mode) const {
    return "_supported_devices_" + toStr(mode) + fileExtension.data();
}

std::string SupportedDevicesHelper::getOclocCurrentVersionOutputFilename() const {
    return getOclocCurrentVersion() + getOutputFilenameSuffix(mode);
}

std::string SupportedDevicesHelper::getOclocCurrentLibName() const {
#ifdef NEO_OCLOC_CURRENT_LIB_NAME
    return std::string(NEO_OCLOC_CURRENT_LIB_NAME);
#else
    return "";
#endif
}

std::string SupportedDevicesHelper::getOclocFormerLibName() const {
#ifdef NEO_OCLOC_FORMER_LIB_NAME
    return std::string(NEO_OCLOC_FORMER_LIB_NAME);
#else
    return "";
#endif
}

std::string SupportedDevicesHelper::getOclocCurrentVersion() const {
    return extractOclocVersion(getOclocCurrentLibName());
}

std::string SupportedDevicesHelper::getOclocFormerVersion() const {
    return extractOclocVersion(getOclocFormerLibName());
}

std::string SupportedDevicesHelper::extractOclocVersion(std::string_view oclocLibNameWithVersion) const {
    // libocloc-2.0.so -> ocloc-2.0
    std::string_view view(oclocLibNameWithVersion);
    auto start = view.find("ocloc-");
    if (start == std::string_view::npos) {
        return "ocloc";
    }

    auto end = view.find(".so", start);
    if (end == std::string_view::npos) {
        return "ocloc";
    }

    return std::string(view.substr(start, end - start));
}

std::string SupportedDevicesHelper::getDataFromFormerOclocVersion() const {
    if (getOclocFormerLibName().empty() ||
        getOclocFormerLibName() == getOclocCurrentLibName()) {
        return "";
    }

    std::unique_ptr<OsLibrary> oclocLib(OsLibrary::load(getOclocFormerLibName()));

    if (!oclocLib ||
        !oclocLib->isLoaded()) {
        return "";
    }

    std::string retData;

    auto oclocInvokeFunc = reinterpret_cast<pOclocInvoke>(oclocLib->getProcAddress("oclocInvoke"));

    const char *argv[] = {"ocloc", "query", "SUPPORTED_DEVICES", "-concat"};

    unsigned int numArgs = sizeof(argv) / sizeof(argv[0]);
    uint32_t numOutputs = 0u;
    unsigned char **dataOutputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    oclocInvokeFunc(numArgs, argv,
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