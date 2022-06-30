/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/program.h"

#include "compiler_options.h"

#include <vector>

namespace NEO {

const std::vector<ConstStringRef> Program::internalOptionsToExtract = {CompilerOptions::gtpinRera,
                                                                       CompilerOptions::greaterThan4gbBuffersRequired};

bool Program::isFlagOption(ConstStringRef option) {
    return true;
}

bool Program::isOptionValueValid(ConstStringRef option, ConstStringRef value) {
    return false;
}

}; // namespace NEO
