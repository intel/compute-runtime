/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class HeapAddressModel : uint32_t {
    /*
     * Private heaps - command list can use any addressing.
     * Heaps are allocated and owned by each command list.
     * Command lists will require to reload SBA command before execution.
     */
    privateHeaps = 0,
    /*
     * Global stateless - command list can use only stateless addressing.
     * Surface state heap is allocated per context for driver's allocations only (debug and scratch)
     * Command lists do not require SBA command reload. SBA is dispatched only once for heap addresses.
     */
    globalStateless = 1,
    /*
     * Global bindless - command list can use stateless or bindless addressing.
     * Surface and dynamic heaps are allocated in global, 4GB allocator for all allocations and samplers used.
     * Command lists do not require SBA command reload. SBA is dispatched only once for heap addresses.
     */
    globalBindless = 2,
    /*
     * Global bindful - command list can use any addressing.
     * Surface and dynamic heaps are allocated in global, 4GB allocator for all allocations and samplers used.
     * Binding table base address is programed by special heap manager that provides and reuses space for bti
     * Command lists might require dynamic binding table base address command reload when binding table heap manager requires to reload base address.
     */
    globalBindful = 3
};

} // namespace NEO
