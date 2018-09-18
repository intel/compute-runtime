/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <atomic>
#include <vector>

namespace OCLRT {

class AddressMapper {
  public:
    AddressMapper();
    ~AddressMapper();

    // maps to continuous region
    uint32_t map(void *vm, size_t size);
    // unmaps
    void unmap(void *vm);

  protected:
    struct MapInfo {
        void *vm;
        size_t size;
        uint32_t ggtt;
    };
    std::vector<MapInfo *> mapping;
    std::atomic<uint32_t> nextPage;
};
} // namespace OCLRT
