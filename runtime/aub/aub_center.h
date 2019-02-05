/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_stream_provider.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/physical_address_allocator.h"
#include "third_party/aub_stream/headers/aub_manager.h"

namespace OCLRT {
struct HardwareInfo;

class AubCenter {
  public:
    AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName);

    AubCenter();
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

    aub_stream::AubManager *getAubManager() const {
        return aubManager.get();
    }

    static uint32_t getAubStreamMode(const std::string &aubFileName, uint32_t csrType);

  protected:
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<AddressMapper> addressMapper;
    std::unique_ptr<AubStreamProvider> streamProvider;

    std::unique_ptr<aub_stream::AubManager> aubManager;
};
} // namespace OCLRT
