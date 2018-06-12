/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/event/event.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "runtime/sampler/sampler.h"

#include <mutex>

// *********************************************************************** //
// *** This file overrides the default implementations of BaseObject   *** //
// *** operator new and delete to track OpenCL object leaks            *** //
// *********************************************************************** //

unsigned int numBaseObjects = 0;
std::mutex numBaseObjectsMutex;

namespace OCLRT {

template <typename B>
void *BaseObject<B>::operator new(size_t sz) {
    std::lock_guard<std::mutex> lock(numBaseObjectsMutex);
    ++numBaseObjects;
    void *ptr = ::operator new(sz);
    return ptr;
}

template <typename B>
void BaseObject<B>::operator delete(void *ptr, size_t) {
    std::lock_guard<std::mutex> lock(numBaseObjectsMutex);
    --numBaseObjects;
    return ::operator delete(ptr);
}

template <typename B>
void *BaseObject<B>::operator new(size_t sz, const std::nothrow_t &tag) noexcept {
    std::lock_guard<std::mutex> lock(numBaseObjectsMutex);
    void *ptr = ::operator new(sz, tag);
    if (ptr)
        ++numBaseObjects;
    return ptr;
}

template <typename B>
void BaseObject<B>::operator delete(void *ptr, const std::nothrow_t &tag) noexcept {
    std::lock_guard<std::mutex> lock(numBaseObjectsMutex);
    --numBaseObjects;
    return ::operator delete(ptr, tag);
}

template class BaseObject<_cl_accelerator_intel>;
template class BaseObject<_cl_command_queue>;
template class BaseObject<_cl_context>;
template class BaseObject<_cl_device_id>;
template class BaseObject<_device_queue>;
template class BaseObject<_cl_event>;
template class BaseObject<_cl_kernel>;
template class BaseObject<_cl_mem>;
template class BaseObject<_cl_platform_id>;
template class BaseObject<_cl_program>;
template class BaseObject<_cl_sampler>;
} // namespace OCLRT
