/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/io_functions.h"

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"

#include <cstring>

namespace NEO {
namespace IoFunctions {
fopenFuncPtr fopenPtr = &fopen;
vfprintfFuncPtr vfprintfPtr = &vfprintf;
vsnprintfFuncPtr vsnprintfPtr = &vsnprintf;
fcloseFuncPtr fclosePtr = &fclose;
getenvFuncPtr getenvPtr = &getenv;
fseekFuncPtr fseekPtr = &fseek;
ftellFuncPtr ftellPtr = &ftell;
rewindFuncPtr rewindPtr = &rewind;
freadFuncPtr freadPtr = &fread;
fwriteFuncPtr fwritePtr = &fwrite;
fflushFuncPtr fflushPtr = &fflush;
mkdirFuncPtr mkdirPtr = &makedir;

char *getEnvironmentVariable(const char *name) {

    char *environmentVariable = getenvPtr(name);

    if (strnlen_s(environmentVariable, CommonConstants::maxAllowedEnvVariableSize) < CommonConstants::maxAllowedEnvVariableSize) {
        return environmentVariable;
    }

    return nullptr;
}

} // namespace IoFunctions
} // namespace NEO
