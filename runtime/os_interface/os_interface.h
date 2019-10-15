/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

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
    static bool osEnabled64kbPages;
    static bool osEnableLocalMemory;
    static bool are64kbPagesEnabled();
    unsigned int getDeviceHandle() const;
    void setGmmInputArgs(void *args);

  protected:
    OSInterfaceImpl *osInterfaceImpl = nullptr;
};
} // namespace NEO
