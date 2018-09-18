/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/test_files.h"

namespace OCLRT {
struct CommandQueueHwFixture;
struct CommandStreamFixture;

// helper functions to enforce MockCompiler input files
inline void overwriteBuiltInBinaryName(
    Device *pDevice,
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

inline void restoreBuiltInBinaryName(Device *pDevice) {
    gEnvironment->igcPopDebugVars();
    gEnvironment->fclPopDebugVars();
}

struct RunKernelFixtureFactory {
    typedef OCLRT::CommandStreamFixture CommandStreamFixture;
    typedef OCLRT::CommandQueueHwFixture CommandQueueFixture;
};

} // namespace OCLRT
