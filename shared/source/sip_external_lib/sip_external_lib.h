/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#include <memory>
#include <string>

namespace NEO {

class SipExternalLib : NonCopyableAndNonMovableClass {
  public:
    virtual ~SipExternalLib() {}
    static SipExternalLib *getSipExternalLibInstance();
    static std::string_view getLibName();

    std::unique_ptr<OsLibrary> libraryHandle = nullptr;
};

} // namespace NEO
