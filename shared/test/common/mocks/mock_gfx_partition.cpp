/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gfx_partition.h"

using namespace NEO;

std::array<HeapIndex, static_cast<uint32_t>(HeapIndex::totalHeaps)>
    MockGfxPartition::allHeapNames{{HeapIndex::heapInternalDeviceMemory,
                                    HeapIndex::heapInternal,
                                    HeapIndex::heapExternalDeviceMemory,
                                    HeapIndex::heapExternal,
                                    HeapIndex::heapStandard,
                                    HeapIndex::heapStandard64KB,
                                    HeapIndex::heapStandard2MB,
                                    HeapIndex::heapSvm}};
