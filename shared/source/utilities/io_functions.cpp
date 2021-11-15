/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/io_functions.h"

namespace NEO {
namespace IoFunctions {
fopenFuncPtr fopenPtr = &fopen;
vfprintfFuncPtr vfprintfPtr = &vfprintf;
fcloseFuncPtr fclosePtr = &fclose;
getenvFuncPtr getenvPtr = &getenv;
fseekFuncPtr fseekPtr = &fseek;
ftellFuncPtr ftellPtr = &ftell;
rewindFuncPtr rewindPtr = &rewind;
freadFuncPtr freadPtr = &fread;
fwriteFuncPtr fwritePtr = &fwrite;
} // namespace IoFunctions
} // namespace NEO
