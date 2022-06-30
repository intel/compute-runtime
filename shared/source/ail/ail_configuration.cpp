/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include "shared/source/helpers/hash.h"

#include <string>
#include <string_view>

namespace NEO {
bool AILConfiguration::isKernelHashCorrect(const std::string &kernelsSources, uint64_t expectedHash) const {
    const auto hash = Hash::hash(kernelsSources.c_str(), kernelsSources.length());
    return hash == expectedHash;
}

bool AILConfiguration::sourcesContainKernel(const std::string &kernelsSources, std::string_view kernelName) const {
    return (kernelsSources.find(kernelName) != std::string::npos);
}
} // namespace NEO
