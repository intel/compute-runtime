/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/os_interface/os_library.h"
#include "os_library.h"
#include "DriverStore.h"

namespace OCLRT {

OsLibrary *OsLibrary::load(const std::string &name) {
    Windows::OsLibrary *ptr = new Windows::OsLibrary(name);

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}

namespace Windows {

OsLibrary::OsLibrary(const std::string &name) {
    if (name.empty()) {
        this->handle = GetModuleHandleA(nullptr);
    } else {
        this->handle = LoadDependency(name.c_str());
        if (this->handle == nullptr) {
            this->handle = ::LoadLibraryA(name.c_str());
        }
    }
}

OsLibrary::~OsLibrary() {
    if ((this->handle != nullptr) && (this->handle != GetModuleHandleA(nullptr))) {
        ::FreeLibrary(this->handle);
        this->handle = nullptr;
    }
}

bool OsLibrary::isLoaded() {
    return this->handle != nullptr;
}

void *OsLibrary::getProcAddress(const std::string &procName) {
    return ::GetProcAddress(this->handle, procName.c_str());
}
}
}
