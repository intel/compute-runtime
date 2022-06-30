/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/sharings/va/va_device.h"
#include "opencl/source/sharings/va/va_sharing_functions.h"
#include "opencl/test/unit_test/linux/mock_os_layer.h"

#include <va/va_backend.h>

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

TEST(VaTests, givenVADeviceWhenGetDeviceFromVAIsCalledThenProperSyscallsAreUsed) {
    VariableBackup<decltype(accessCalledTimes)> backupAccessCalledTimes(&accessCalledTimes);
    VariableBackup<decltype(readLinkCalledTimes)> backupReadLinkCalledTimes(&readLinkCalledTimes);
    VariableBackup<decltype(fstatCalledTimes)> backupFstatCalledTimes(&fstatCalledTimes);
    accessCalledTimes = 0;
    readLinkCalledTimes = 0;
    fstatCalledTimes = 0;

    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    VADevice vaDevice{};
    auto clDevice = vaDevice.getDeviceFromVA(nullptr, vaDisplay.get());
    EXPECT_EQ(clDevice, nullptr);

    EXPECT_EQ(accessCalledTimes, 1);
    EXPECT_EQ(readLinkCalledTimes, 1);
    EXPECT_EQ(fstatCalledTimes, 1);
}
