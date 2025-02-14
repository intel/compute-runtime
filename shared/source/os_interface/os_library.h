/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

namespace NEO {

struct ConvertibleProcAddr {
    template <typename T>
    operator T *() const {
        static_assert(std::is_function<T>::value, "Cannot convert to non-function and non-void* type");
        return reinterpret_cast<T *>(ptr);
    }

    operator void *() const {
        return ptr;
    }
    void *ptr = nullptr;
};

struct OsLibraryCreateProperties {
    OsLibraryCreateProperties(const std::string &name) : libraryName(name) {}
    std::string libraryName;
    std::string *errorValue = nullptr;
    bool performSelfLoad = false;
    int *customLoadFlags = nullptr;
};

class OsLibrary {
  protected:
    OsLibrary() = default;
    static OsLibrary *load(const OsLibraryCreateProperties &properties);

  public:
    virtual ~OsLibrary() = default;

    static decltype(&OsLibrary::load) loadFunc;
    static const std::string createFullSystemPath(const std::string &name);

    ConvertibleProcAddr operator[](const std::string &name) {
        return ConvertibleProcAddr{getProcAddress(name)};
    }
    virtual void *getProcAddress(const std::string &procName) = 0;
    virtual bool isLoaded() = 0;
    virtual std::string getFullPath() = 0;
};

bool getLoadedLibVersion(const std::string &libName, const std::string &regexVersionPattern, std::string &outVersion, std::string &errReason);

} // namespace NEO
