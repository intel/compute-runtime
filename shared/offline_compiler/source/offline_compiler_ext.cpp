/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/offline_compiler_ext.h"

#include "shared/offline_compiler/source/offline_compiler.h"

namespace NEO {

std::string getOfflineCompilerOptionsExt() {
    return "";
}

int OfflineCompiler::parseCommandLineExt(size_t numArgs, const std::vector<std::string> &argv, uint32_t &argIndex) {
    return OCLOC_INVALID_COMMAND_LINE;
}

bool isIrOnly(const std::vector<std::string> &args) {
    return std::find(args.begin(), args.end(), "-spv_only") != args.end();
}

} // namespace NEO
