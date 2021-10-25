/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"

namespace NEO {
struct CommandQueueHwFixture;
struct CommandStreamFixture;

// helper functions to enforce MockCompiler input files
inline void overwriteBuiltInBinaryName(
    const std::string &filename,
    bool appendOptionsToFileName = false) {
    // set mock compiler to return expected kernel...
    MockCompilerDebugVars fclDebugVars;
    MockCompilerDebugVars igcDebugVars;

    retrieveBinaryKernelFilename(fclDebugVars.fileName, filename + "_", ".bc");
    fclDebugVars.appendOptionsToFileName = appendOptionsToFileName;

    retrieveBinaryKernelFilename(igcDebugVars.fileName, filename + "_", ".gen");
    igcDebugVars.appendOptionsToFileName = appendOptionsToFileName;

    gEnvironment->fclPushDebugVars(fclDebugVars);
    gEnvironment->igcPushDebugVars(igcDebugVars);
}

inline void restoreBuiltInBinaryName() {
    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}

struct RunKernelFixtureFactory {
    typedef NEO::CommandStreamFixture CommandStreamFixture;
    typedef NEO::CommandQueueHwFixture CommandQueueFixture;
};

} // namespace NEO
