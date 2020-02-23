/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/va/va_sharing_functions.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "test.h"

using namespace NEO;

TEST(VaTests, whenLibvaSo2IsNotInstalledThenFail) {
    VariableBackup<decltype(VASharingFunctions::fdlopen)> dlopenBackup(&VASharingFunctions::fdlopen);
    VariableBackup<decltype(VASharingFunctions::fdlclose)> dlcloseBackup(&VASharingFunctions::fdlclose);
    VariableBackup<decltype(VASharingFunctions::fdlsym)> dlsymBackup(&VASharingFunctions::fdlsym);

    VASharingFunctions::fdlopen = [&](const char *filename, int flag) -> void * {
        if (!strncmp(filename, "libva.so.2", 10)) {
            return (void *)0xdeadbeef;
        } else
            return 0;
    };
    VASharingFunctions::fdlclose = [&](void *handle) -> int {
        return 0;
    };

    VASharingFunctions::fdlsym = [&](void *handle, const char *symbol) -> void * {
        return nullptr;
    };

    VADisplay vaDisplay = nullptr;
    VASharingFunctions va(vaDisplay);

    EXPECT_EQ(true, va.isVaLibraryAvailable());
}
