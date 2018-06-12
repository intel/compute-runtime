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

#include "runtime/os_interface/os_library.h"
#include "common/gtsysinfo.h"
#include "igfxfmid.h"

namespace Os {
///////////////////////////////////////////////////////////////////////////////
// These options determine the Windows specific behavior for
// the runtime unit tests
///////////////////////////////////////////////////////////////////////////////
const char *frontEndDllName = "mock_igdfcl.dll";
const char *igcDllName = "mock_igc.dll";
const char *gdiDllName = "gdi32_mock.dll";
const char *gmmDllName = "mock_gmm.dll";
const char *testDllName = "test_dynamic_lib.dll";
} // namespace Os

OCLRT::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo) {
    OCLRT::OsLibrary *mockGdiDll;
    mockGdiDll = OCLRT::OsLibrary::load("gdi32_mock.dll");

    typedef void(__stdcall * pfSetAdapterInfo)(const void *, const void *);
    pfSetAdapterInfo setAdpaterInfo = reinterpret_cast<pfSetAdapterInfo>(mockGdiDll->getProcAddress("MockSetAdapterInfo"));

    setAdpaterInfo(platform, gtSystemInfo);

    return mockGdiDll;
}
