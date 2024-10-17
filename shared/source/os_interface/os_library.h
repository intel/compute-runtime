/*
 * Copyright (C) 2018-2024 Intel Corporation
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

class OsLibrary {
  protected:
    OsLibrary() = default;
    static OsLibrary *load(const std::string &name) { return loadAndCaptureError(name, nullptr); }

  public:
    virtual ~OsLibrary() = default;

    static const int *loadFlagsOverwrite;

    static decltype(&OsLibrary::load) loadFunc;
    static OsLibrary *loadAndCaptureError(const std::string &name, std::string *errorValue);
    static const std::string createFullSystemPath(const std::string &name);

    ConvertibleProcAddr operator[](const std::string &name) {
        return ConvertibleProcAddr{getProcAddress(name)};
    }
    virtual void *getProcAddress(const std::string &procName) = 0;
    virtual bool isLoaded() = 0;
    virtual std::string getFullPath() = 0;
};
} // namespace NEO
