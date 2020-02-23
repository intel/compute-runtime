/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_gfx_partition.h"

using namespace NEO;

std::array<HeapIndex, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)>
    MockGfxPartition::allHeapNames{{HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY,
                                    HeapIndex::HEAP_INTERNAL,
                                    HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY,
                                    HeapIndex::HEAP_EXTERNAL,
                                    HeapIndex::HEAP_STANDARD,
                                    HeapIndex::HEAP_STANDARD64KB,
                                    HeapIndex::HEAP_SVM}};
