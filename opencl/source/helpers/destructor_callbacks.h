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
class DestructorCallbacks {
    using CallbackType = void CL_CALLBACK(T, void *);

  public:
    inline void add(CallbackType *callback, void *userData) {
        callbacks.push_back({callback, userData});
    }
    inline bool empty() {
        return callbacks.empty();
    }
    inline void invoke(T object) {
        for (auto it = callbacks.rbegin(); it != callbacks.rend(); it++) {
            it->first(object, it->second);
        }
    }

  private:
    std::vector<std::pair<CallbackType *, void *>> callbacks;
};

using ContextDestructorCallbacks = DestructorCallbacks<cl_context>;
using MemObjDestructorCallbacks = DestructorCallbacks<cl_mem>;

} // namespace NEO
