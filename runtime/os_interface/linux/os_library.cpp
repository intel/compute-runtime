/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_library.h"

#include "core/helpers/debug_helpers.h"

#include "os_library.h"

#include <dlfcn.h>

namespace NEO {
OsLibrary *OsLibrary::load(const std::string &name) {
    auto ptr = new (std::nothrow) Linux::OsLibrary(name);
    if (ptr == nullptr)
        return nullptr;

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}
namespace Linux {

OsLibrary::OsLibrary(const std::string &name) {
    if (name.empty()) {
        this->handle = dlopen(0, RTLD_LAZY);
    } else {
#ifdef SANITIZER_BUILD
        constexpr auto dlopenFlag = RTLD_LAZY;
#else
        constexpr auto dlopenFlag = RTLD_LAZY | RTLD_DEEPBIND;
#endif
        this->handle = dlopen(name.c_str(), dlopenFlag);
    }
}

OsLibrary::~OsLibrary() {
    if (this->handle != nullptr) {
        dlclose(this->handle);
        this->handle = nullptr;
    }
}

bool OsLibrary::isLoaded() {
    return this->handle != nullptr;
}

void *OsLibrary::getProcAddress(const std::string &procName) {
    DEBUG_BREAK_IF(this->handle == nullptr);

    return dlsym(this->handle, procName.c_str());
}
} // namespace Linux
} // namespace NEO
