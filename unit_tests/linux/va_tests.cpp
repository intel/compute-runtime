/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/va/va_sharing_functions.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

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
