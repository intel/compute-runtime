/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"
#include "shared/source/utilities/io_functions.h"
namespace NEO {
const char *eudebugSysfsEntry[static_cast<uint32_t>(EuDebugInterfaceType::maxValue)]{};
EuDebugInterfaceCreateFunctionType eudebugInterfaceFactory[static_cast<uint32_t>(EuDebugInterfaceType::maxValue)]{};

std::unique_ptr<EuDebugInterface> EuDebugInterface::create(const std::string &sysFsPciPath) {
    char enabledEuDebug = '0';

    for (auto i = 0u; i < static_cast<uint32_t>(EuDebugInterfaceType::maxValue); i++) {
        if (eudebugSysfsEntry[i]) {
            auto path = sysFsPciPath + eudebugSysfsEntry[i];
            FILE *fileDescriptor = IoFunctions::fopenPtr(path.c_str(), "r");
            if (fileDescriptor) {
                [[maybe_unused]] auto bytesRead = IoFunctions::freadPtr(&enabledEuDebug, 1, 1, fileDescriptor);
                IoFunctions::fclosePtr(fileDescriptor);
                if (enabledEuDebug == '1') {
                    return eudebugInterfaceFactory[i]();
                }
            }
        }
    }
    return {};
}
} // namespace NEO
