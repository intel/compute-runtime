/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {

class OSInterface {

  public:
    class OSInterfaceImpl;
    OSInterface();
    virtual ~OSInterface();
    OSInterface(const OSInterface &) = delete;
    OSInterface &operator=(const OSInterface &) = delete;

    OSInterfaceImpl *get() const {
        return osInterfaceImpl;
    };
    unsigned int getHwContextId() const;
    static bool osEnabled64kbPages;
    static bool are64kbPagesEnabled();
    unsigned int getDeviceHandle() const;

  protected:
    OSInterfaceImpl *osInterfaceImpl = nullptr;
};
} // namespace OCLRT
