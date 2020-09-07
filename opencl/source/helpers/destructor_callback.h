/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

namespace NEO {

template <typename T>
class DestructorCallback {
  public:
    DestructorCallback(void(CL_CALLBACK *funcNotify)(T, void *),
                       void *userData)
        : funcNotify(funcNotify), userData(userData){};

    inline void invoke(T object) {
        this->funcNotify(object, userData);
    }

  private:
    void(CL_CALLBACK *funcNotify)(T, void *);
    void *userData;
};

using ContextDestructorCallback = DestructorCallback<cl_context>;
using MemObjDestructorCallback = DestructorCallback<cl_mem>;
using ProgramReleaseCallback = DestructorCallback<cl_program>;

} // namespace NEO
