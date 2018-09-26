/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_stream_provider.h"
#include "runtime/memory_manager/physical_address_allocator.h"

namespace OCLRT {

class AubCenter {
  public:
    AubCenter() {
        streamProvider = std::make_unique<AubFileStreamProvider>();
    }
    virtual ~AubCenter() = default;

    void initPhysicalAddressAllocator(PhysicalAddressAllocator *pPhysicalAddressAllocator) {
        physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(pPhysicalAddressAllocator);
    }

    PhysicalAddressAllocator *getPhysicalAddressAllocator() const {
        return physicalAddressAllocator.get();
    }

    AubStreamProvider *getStreamProvider() const {
        return streamProvider.get();
    }

  protected:
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<AubStreamProvider> streamProvider;
};
} // namespace OCLRT
