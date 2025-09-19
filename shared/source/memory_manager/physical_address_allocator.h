/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/memory_banks.h"

#include <atomic>
#include <mutex>

namespace NEO {

class PhysicalAddressAllocator {
  public:
    PhysicalAddressAllocator() {
        mainAllocator.store(initialPageAddress);
    }

    virtual ~PhysicalAddressAllocator() = default;

    uint64_t reserve4kPage(uint32_t memoryBank) {
        return reservePage(memoryBank, MemoryConstants::pageSize, MemoryConstants::pageSize);
    }

    uint64_t reserve64kPage(uint32_t memoryBank) {
        return reservePage(memoryBank, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k);
    }

    virtual uint64_t reservePage(uint32_t memoryBank, size_t pageSize, size_t alignment) {
        UNRECOVERABLE_IF(memoryBank != MemoryBanks::mainBank);

        std::unique_lock<std::mutex> lock(pageReserveMutex);

        auto currentAddress = mainAllocator.load();
        auto alignmentSize = alignUp(currentAddress, alignment) - currentAddress;
        mainAllocator += alignmentSize;
        return mainAllocator.fetch_add(pageSize);
    }

  protected:
    std::atomic<uint64_t> mainAllocator;
    std::mutex pageReserveMutex;
    const uint64_t initialPageAddress = 0x1000;
};

template <typename GfxFamily>
class PhysicalAddressAllocatorHw : public PhysicalAddressAllocator {

  public:
    PhysicalAddressAllocatorHw(uint64_t bankSize, uint32_t numOfBanks) : memoryBankSize(bankSize), numberOfBanks(numOfBanks) {
        if (numberOfBanks > 0) {
            bankAllocators = new std::atomic<uint64_t>[numberOfBanks];
            bankAllocators[0].store(initialPageAddress);

            for (uint32_t i = 1; i < numberOfBanks; i++) {
                bankAllocators[i].store(i * memoryBankSize);
            }
        }
    }

    ~PhysicalAddressAllocatorHw() override {
        if (bankAllocators) {
            delete[] bankAllocators;
        }
    }

    uint64_t reservePage(uint32_t memoryBank, size_t pageSize, size_t alignment) override {
        std::unique_lock<std::mutex> lock(pageReserveMutex);

        if (memoryBank == MemoryBanks::mainBank || numberOfBanks == 0) {
            auto currentAddress = mainAllocator.load();
            auto alignmentSize = alignUp(currentAddress, alignment) - currentAddress;
            mainAllocator += alignmentSize;
            return mainAllocator.fetch_add(pageSize);
        }
        UNRECOVERABLE_IF(memoryBank > numberOfBanks);

        auto index = memoryBank - MemoryBanks::getBankForLocalMemory(0);

        auto currentAddress = bankAllocators[index].load();
        auto alignmentSize = alignUp(currentAddress, alignment) - currentAddress;
        bankAllocators[index] += alignmentSize;

        auto address = bankAllocators[index].fetch_add(pageSize);

        UNRECOVERABLE_IF(address > ((index + 1) * memoryBankSize));

        return address;
    }

    uint64_t getBankSize() { return memoryBankSize; }
    uint32_t getNumberOfBanks() { return numberOfBanks; }

  protected:
    std::atomic<uint64_t> *bankAllocators = nullptr;
    uint64_t memoryBankSize = 0;
    uint32_t numberOfBanks = 0;
};

} // namespace NEO
