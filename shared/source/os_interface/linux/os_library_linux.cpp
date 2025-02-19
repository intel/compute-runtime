/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_library_linux.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <cstring>
#include <dlfcn.h>
#include <link.h>

namespace NEO {

OsLibrary *OsLibrary::load(const OsLibraryCreateProperties &properties) {
    auto ptr = new (std::nothrow) Linux::OsLibrary(properties);
    if (ptr == nullptr)
        return nullptr;

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}

const std::string OsLibrary::createFullSystemPath(const std::string &name) {
    return name;
}

bool getLoadedLibVersion(const std::string &libName, const std::string &regexVersionPattern, std::string &outVersion, std::string &errReason) {
    return false;
}

namespace Linux {

OsLibrary::OsLibrary(const OsLibraryCreateProperties &properties) {
    if (properties.libraryName.empty() || properties.performSelfLoad) {
        this->handle = SysCalls::dlopen(0, RTLD_LAZY);
    } else {
#ifdef SANITIZER_BUILD
        auto dlopenFlag = RTLD_LAZY;
#else
        auto dlopenFlag = RTLD_LAZY | RTLD_DEEPBIND;
        /* Background: https://github.com/intel/compute-runtime/issues/122 */
#endif
        dlopenFlag = properties.customLoadFlags ? *properties.customLoadFlags : dlopenFlag;
        adjustLibraryFlags(dlopenFlag);
        this->handle = SysCalls::dlopen(properties.libraryName.c_str(), dlopenFlag);
        if (!this->handle && (properties.errorValue != nullptr)) {
            properties.errorValue->assign(dlerror());
        }
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

std::string OsLibrary::getFullPath() {
    struct link_map *map = nullptr;
    int retVal = NEO::SysCalls::dlinfo(this->handle, RTLD_DI_LINKMAP, &map);
    if (retVal == 0 && map != nullptr) {
        return std::string(map->l_name);
    }
    return std::string();
}

bool isLibraryLoaded(const std::string &libraryName) {
    auto handle = SysCalls::dlopen(0, RTLD_LAZY);
    if (!handle) {
        return false;
    }
    struct link_map *map = nullptr;
    int retVal = NEO::SysCalls::dlinfo(handle, RTLD_DI_LINKMAP, &map);
    if (retVal == 0 && map != nullptr) {
        while (map) {
            if (strstr(map->l_name, libraryName.c_str())) {
                dlclose(handle);
                return true;
            }
            map = map->l_next;
        }
    }
    dlclose(handle);
    return false;
}
} // namespace Linux
} // namespace NEO
