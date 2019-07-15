/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/helpers/array_count.h"
#include "runtime/sharings/va/va_sharing_functions.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/sharings/va/mock_va_sharing.h"

#include "gtest/gtest.h"

#include <functional>
#include <memory>

using namespace NEO;

class VASharingFunctionsTested : public VASharingFunctions {
  public:
    VASharingFunctionsTested() : VASharingFunctions(nullptr) {}

    bool wereFunctionsAssigned() const {
        return vaDisplayIsValidPFN != nullptr &&
               vaDeriveImagePFN != nullptr &&
               vaDestroyImagePFN != nullptr &&
               vaSyncSurfacePFN != nullptr &&
               vaGetLibFuncPFN != nullptr &&
               vaExtGetSurfaceHandlePFN != nullptr;
    }

    bool wereFunctionsAssignedNull() const {
        return vaDisplayIsValidPFN == nullptr &&
               vaDeriveImagePFN == nullptr &&
               vaDestroyImagePFN == nullptr &&
               vaSyncSurfacePFN == nullptr &&
               vaGetLibFuncPFN == nullptr &&
               vaExtGetSurfaceHandlePFN == nullptr;
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

TEST(VASharingFunctions, givenEnabledExtendedVaFormatsWhenQueryingSupportedFormatsThenAllSupportedFormatsAreStored) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableExtendedVaFormats.set(true);

    VASharingFunctionsMock sharingFunctions;

    sharingFunctions.querySupportedVaImageFormats(VADisplay(1));

    EXPECT_EQ(2u, sharingFunctions.supportedFormats.size());

    size_t allFormatsFound = 0;
    uint32_t fourCCExpected[] = {VA_FOURCC_NV12, VA_FOURCC_P010};

    for (size_t i = 0; i < sharingFunctions.supportedFormats.size(); i++) {
        for (size_t fourCCIndex = 0; fourCCIndex < arrayCount(fourCCExpected); fourCCIndex++) {
            if (sharingFunctions.supportedFormats[0].fourcc == fourCCExpected[fourCCIndex]) {
                allFormatsFound++;
            }
        }
    }
    EXPECT_EQ(2u, allFormatsFound);
}
