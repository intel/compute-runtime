/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/physical_address_allocator.h"

using namespace NEO;

class MockPhysicalAddressAllocator : public PhysicalAddressAllocator {
  public:
    using PhysicalAddressAllocator::initialPageAddress;
    using PhysicalAddressAllocator::mainAllocator;
    using PhysicalAddressAllocator::PhysicalAddressAllocator;
};
