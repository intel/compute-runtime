/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/os_library.h"
#include "os_library.h"
#include <dlfcn.h>

namespace OCLRT {
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
        this->handle = dlopen(name.c_str(), RTLD_LAZY);
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
} // namespace OCLRT
