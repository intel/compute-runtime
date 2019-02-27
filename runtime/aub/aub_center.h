/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_stream_provider.h"
#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/physical_address_allocator.h"

#include "third_party/aub_stream/headers/aub_manager.h"

namespace OCLRT {
struct HardwareInfo;

class AubCenter {
  public:
    AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);

    AubCenter();
    virtual ~AubCenter();

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

    AubSubCaptureManager *getSubCaptureManager() const {
        return subCaptureManager.get();
    }

    aub_stream::AubManager *getAubManager() const {
        return aubManager.get();
    }

    static uint32_t getAubStreamMode(const std::string &aubFileName, uint32_t csrType);

  protected:
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<AddressMapper> addressMapper;
    std::unique_ptr<AubStreamProvider> streamProvider;

    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
    std::unique_ptr<aub_stream::AubManager> aubManager;
    uint32_t aubStreamMode = 0;
};
} // namespace OCLRT
