/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <string>
#include <string_view>

namespace NEO {

const char legacyPlatformName[] = "Intel(R) OpenCL";

bool AILConfiguration::isKernelHashCorrect(const std::string &kernelsSources, uint64_t expectedHash) const {
    const auto hash = Hash::hash(kernelsSources.c_str(), kernelsSources.length());
    return hash == expectedHash;
}

bool AILConfiguration::sourcesContain(const std::string &sources, std::string_view contentToFind) const {
    return (sources.find(contentToFind) != std::string::npos);
}

} // namespace NEO
