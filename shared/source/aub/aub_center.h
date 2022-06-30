/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_stream_provider.h"
#include "shared/source/aub/aub_subcapture.h"
#include "shared/source/helpers/options.h"
#include "shared/source/memory_manager/address_mapper.h"
#include "shared/source/memory_manager/physical_address_allocator.h"

#include "third_party/aub_stream/headers/aub_manager.h"

namespace NEO {
struct HardwareInfo;
class GmmHelper;

class AubCenter {
  public:
    AubCenter(const HardwareInfo *pHwInfo, const GmmHelper &gmmHelper, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);

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

    AubSubCaptureCommon *getSubCaptureCommon() const {
        return subCaptureCommon.get();
    }

    aub_stream::AubManager *getAubManager() const {
        return aubManager.get();
    }

    static uint32_t getAubStreamMode(const std::string &aubFileName, uint32_t csrType);

  protected:
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<AddressMapper> addressMapper;
    std::unique_ptr<AubStreamProvider> streamProvider;

    std::unique_ptr<AubSubCaptureCommon> subCaptureCommon;
    std::unique_ptr<aub_stream::AubManager> aubManager;
    uint32_t aubStreamMode = 0;
    uint32_t stepping = 0;
};
} // namespace NEO
