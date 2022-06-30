/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_io_functions.h"

namespace NEO {
namespace IoFunctions {
fopenFuncPtr fopenPtr = &mockFopen;
vfprintfFuncPtr vfprintfPtr = &mockVfptrinf;
fcloseFuncPtr fclosePtr = &mockFclose;
getenvFuncPtr getenvPtr = &mockGetenv;
fseekFuncPtr fseekPtr = &mockFseek;
ftellFuncPtr ftellPtr = &mockFtell;
rewindFuncPtr rewindPtr = &mockRewind;
freadFuncPtr freadPtr = &mockFread;
fwriteFuncPtr fwritePtr = &mockFwrite;

uint32_t mockFopenCalled = 0;
FILE *mockFopenReturned = reinterpret_cast<FILE *>(0x40);
uint32_t failAfterNFopenCount = 0;
uint32_t mockVfptrinfCalled = 0;
uint32_t mockFcloseCalled = 0;
uint32_t mockGetenvCalled = 0;
uint32_t mockFseekCalled = 0;
uint32_t mockFtellCalled = 0;
long int mockFtellReturn = 0;
uint32_t mockRewindCalled = 0;
uint32_t mockFreadCalled = 0;
size_t mockFreadReturn = 0;
uint32_t mockFwriteCalled = 0;
size_t mockFwriteReturn = 0;

std::unordered_map<std::string, std::string> *mockableEnvValues = nullptr;

} // namespace IoFunctions
} // namespace NEO
