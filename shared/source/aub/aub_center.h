/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_subcapture.h"
#include "shared/source/helpers/options.h"
#include "shared/source/memory_manager/physical_address_allocator.h"

#include "aubstream/aub_manager.h"

namespace NEO {
struct RootDeviceEnvironment;

class AubCenter {
  public:
    AubCenter(const RootDeviceEnvironment &rootDeviceEnvironment, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);

    AubCenter();
    virtual ~AubCenter() = default;

    void initPhysicalAddressAllocator(PhysicalAddressAllocator *pPhysicalAddressAllocator) {
        physicalAddressAllocator = std::unique_ptr<PhysicalAddressAllocator>(pPhysicalAddressAllocator);
    }

    PhysicalAddressAllocator *getPhysicalAddressAllocator() const {
        return physicalAddressAllocator.get();
    }

    AubSubCaptureCommon *getSubCaptureCommon() const {
        return subCaptureCommon.get();
    }

    aub_stream::AubManager *getAubManager() const {
        return aubManager.get();
    }

    static uint32_t getAubStreamMode(const std::string &aubFileName, CommandStreamReceiverType csrType);

  protected:
    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;

    std::unique_ptr<AubSubCaptureCommon> subCaptureCommon;
    std::unique_ptr<aub_stream::AubManager> aubManager;
    uint32_t aubStreamMode = 0;
    uint32_t stepping = 0;
};
} // namespace NEO
