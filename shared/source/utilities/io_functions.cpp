/*
 * Copyright (C) 2020-2023 Intel Corporation
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
fflushFuncPtr fflushPtr = &fflush;
} // namespace IoFunctions
} // namespace NEO
