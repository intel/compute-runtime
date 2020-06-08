/*
 * Copyright (C) 2020 Intel Corporation
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
} // namespace IoFunctions
} // namespace NEO
