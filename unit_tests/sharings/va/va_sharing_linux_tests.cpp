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

#include "runtime/sharings/va/va_sharing_functions.h"
#include "gtest/gtest.h"
#include "unit_tests/helpers/variable_backup.h"

#include <functional>
#include <memory>

using namespace OCLRT;

class VASharingFunctionsTested : public VASharingFunctions {
  public:
    VASharingFunctionsTested() : VASharingFunctions(nullptr) {}

    bool wereFunctionsAssigned() {
        if (
            vaDisplayIsValidPFN != nullptr &&
            vaDeriveImagePFN != nullptr &&
            vaDestroyImagePFN != nullptr &&
            vaSyncSurfacePFN != nullptr &&
            vaGetLibFuncPFN != nullptr &&
            vaExtGetSurfaceHandlePFN != nullptr) {
            return true;
        }
        return false;
    }

    bool wereFunctionsAssignedNull() {
        if (
            vaDisplayIsValidPFN == nullptr &&
            vaDeriveImagePFN == nullptr &&
            vaDestroyImagePFN == nullptr &&
            vaSyncSurfacePFN == nullptr &&
            vaGetLibFuncPFN == nullptr &&
            vaExtGetSurfaceHandlePFN == nullptr) {
            return true;
        }
        return false;
    }
};

TEST(VASharingFunctions, GivenInitFunctionsWhenDLOpenFailsThenFunctionsAreNull) {
    VariableBackup<decltype(VASharingFunctions::fdlopen)> dlopenBackup(&VASharingFunctions::fdlopen);
    VariableBackup<decltype(VASharingFunctions::fdlsym)> dlsymBackup(&VASharingFunctions::fdlsym);
    VariableBackup<decltype(VASharingFunctions::fdlclose)> dlcloseBackup(&VASharingFunctions::fdlclose);

    VASharingFunctions::fdlopen = [&](const char *filename, int flag) -> void * {
        return nullptr;
    };

    VASharingFunctions::fdlsym = [&](void *handle, const char *symbol) -> void * {
        return nullptr;
    };

    VASharingFunctions::fdlclose = [&](void *handle) -> int {
        return 0;
    };

    VASharingFunctionsTested functions;
    EXPECT_TRUE(functions.wereFunctionsAssignedNull());
}

void *GetLibFunc(VADisplay vaDisplay, const char *func) {
    return (void *)0xdeadbeef;
}

TEST(VASharingFunctions, GivenInitFunctionsWhenDLOpenSuccedsThenFunctionsAreNotNull) {
    VariableBackup<decltype(VASharingFunctions::fdlopen)> dlopenBackup(&VASharingFunctions::fdlopen);
    VariableBackup<decltype(VASharingFunctions::fdlsym)> dlsymBackup(&VASharingFunctions::fdlsym);
    VariableBackup<decltype(VASharingFunctions::fdlclose)> dlcloseBackup(&VASharingFunctions::fdlclose);

    std::unique_ptr<uint32_t> valib(new uint32_t);
    ASSERT_NE(nullptr, valib.get());

    VASharingFunctions::fdlopen = [&](const char *filename, int flag) -> void * {
        return valib.get();
    };

    VASharingFunctions::fdlsym = [&](void *handle, const char *symbol) -> void * {
        return (void *)GetLibFunc;
    };

    VASharingFunctions::fdlclose = [&](void *handle) -> int {
        return 0;
    };

    VASharingFunctionsTested functions;
    EXPECT_TRUE(functions.wereFunctionsAssigned());
}

TEST(VASharingFunctions, GivenFunctionsWhenNoLibvaThenDlcloseNotCalled) {
    VariableBackup<decltype(VASharingFunctions::fdlopen)> dlopenBackup(&VASharingFunctions::fdlopen);
    VariableBackup<decltype(VASharingFunctions::fdlsym)> dlsymBackup(&VASharingFunctions::fdlsym);
    VariableBackup<decltype(VASharingFunctions::fdlclose)> dlcloseBackup(&VASharingFunctions::fdlclose);

    uint32_t closeCalls = 0;

    VASharingFunctions::fdlopen = [&](const char *filename, int flag) -> void * {
        return nullptr;
    };

    VASharingFunctions::fdlsym = [&](void *handle, const char *symbol) -> void * {
        return nullptr;
    };

    VASharingFunctions::fdlclose = [&](void *handle) -> int {
        closeCalls++;
        return 0;
    };

    {
        // we need this to properly track closeCalls
        VASharingFunctionsTested functions;
    }
    EXPECT_EQ(0u, closeCalls);
}

TEST(VASharingFunctions, GivenFunctionsWhenLibvaLoadedThenDlcloseIsCalled) {
    VariableBackup<decltype(VASharingFunctions::fdlopen)> dlopenBackup(&VASharingFunctions::fdlopen);
    VariableBackup<decltype(VASharingFunctions::fdlsym)> dlsymBackup(&VASharingFunctions::fdlsym);
    VariableBackup<decltype(VASharingFunctions::fdlclose)> dlcloseBackup(&VASharingFunctions::fdlclose);

    std::unique_ptr<uint32_t> valib(new uint32_t);
    ASSERT_NE(nullptr, valib.get());

    uint32_t closeCalls = 0;

    VASharingFunctions::fdlopen = [&](const char *filename, int flag) -> void * {
        return valib.get();
    };

    VASharingFunctions::fdlsym = [&](void *handle, const char *symbol) -> void * {
        return nullptr;
    };

    VASharingFunctions::fdlclose = [&](void *handle) -> int {
        if (handle == valib.get()) {
            closeCalls++;
        }
        return 0;
    };

    {
        // we need this to properly track closeCalls
        VASharingFunctionsTested functions;
    }
    EXPECT_EQ(1u, closeCalls);
}
