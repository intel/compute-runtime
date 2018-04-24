/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
