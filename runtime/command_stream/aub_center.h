/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_stream_provider.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/physical_address_allocator.h"

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/aub_streamer.h"

namespace OCLRT {

class AubCenter {
  public:
    AubCenter() {
        if (DebugManager.flags.UseAubStream.get()) {
            std::string filename("aub.aub");
            aubManager = std::make_unique<AubDump::AubManager>(1, false, filename);
        }
        addressMapper = std::make_unique<AddressMapper>();
        streamProvider = std::make_unique<AubFileStreamProvider>();
    }
    virtual ~AubCenter() = default;

    void initPhysicalAddressAllocator(PhysicalAddressAllocator *pPhysicalAddressAllocator) {
        physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(pPhysicalAddressAllocator);
    }

    PhysicalAddressAllocator *getPhysicalAddressAllocator() const {
        return physicalAddressAllocator.get();
    }

    AddressMapper *getAddressMapper() const {
        return addressMapper.get();
    }

    AubStreamProvider *getStreamProvider() const {
        return streamProvider.get();
    }

  protected:
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<AddressMapper> addressMapper;
    std::unique_ptr<AubStreamProvider> streamProvider;

    std::unique_ptr<AubDump::AubManager> aubManager;
};
} // namespace OCLRT
