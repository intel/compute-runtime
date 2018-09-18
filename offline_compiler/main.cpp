/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/offline_compiler.h"
#include "offline_compiler/utilities/safety_caller.h"
#include "runtime/os_interface/os_library.h"

#include <CL/cl.h>

using namespace OCLRT;

int main(int numArgs, const char *argv[]) {
    int retVal = CL_SUCCESS;
    OfflineCompiler *pCompiler = OfflineCompiler::create(numArgs, argv, retVal);

    if (retVal == CL_SUCCESS) {
        retVal = buildWithSafetyGuard(pCompiler);

        std::string buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            printf("%s\n", buildLog.c_str());
        }

        if (retVal == CL_SUCCESS) {
            if (!pCompiler->isQuiet())
                printf("Build succeeded.\n");
        } else {
            printf("Build failed with error code: %d\n", retVal);
        }
    }

    delete pCompiler;
    return retVal;
}
