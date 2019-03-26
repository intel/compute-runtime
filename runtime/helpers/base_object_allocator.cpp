/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

// *********************************************************************** //
// *** PLEASE LIMIT THIS FILE TO THE IMPLEMENTATIONS OF new AND delete *** //
// *** Unit tests override this default implementation to track leaks  *** //
// *** Any additional logic or dependencies may break this feature     *** //
// *********************************************************************** //

namespace NEO {

template <typename B>
void *BaseObject<B>::operator new(size_t sz) {
    return ::operator new(sz);
}

template <typename B>
void BaseObject<B>::operator delete(void *ptr, size_t allocationSize) {
    return ::operator delete(ptr);
}

template <typename B>
void *BaseObject<B>::operator new(size_t sz, const std::nothrow_t &tag) noexcept {
    return ::operator new(sz, tag);
}

template <typename B>
void BaseObject<B>::operator delete(void *ptr, const std::nothrow_t &tag) noexcept {
    return ::operator delete(ptr, tag);
}

template class BaseObject<_cl_accelerator_intel>;
template class BaseObject<_cl_command_queue>;
template class BaseObject<_device_queue>;
template class BaseObject<_cl_context>;
template class BaseObject<_cl_device_id>;
template class BaseObject<_cl_event>;
template class BaseObject<_cl_kernel>;
template class BaseObject<_cl_mem>;
template class BaseObject<_cl_platform_id>;
template class BaseObject<_cl_program>;
template class BaseObject<_cl_sampler>;
} // namespace NEO
