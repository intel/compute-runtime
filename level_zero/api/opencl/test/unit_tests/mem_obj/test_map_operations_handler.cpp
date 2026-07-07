/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/mem_obj/leo_map_operations_handler.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(MapOperationsHandlerTests, givenNewHandlerWhenQuerySizeThenReturnsZero) {
    MapOperationsHandler handler;
    EXPECT_EQ(0u, handler.size());
}

TEST(MapOperationsHandlerTests, givenSingleAddWhenQuerySizeThenReturnsOne) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0x1000);
    cl_map_flags flags = CL_MAP_WRITE;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    EXPECT_TRUE(handler.add(ptr, 16, flags, size, offset));
    EXPECT_EQ(1u, handler.size());
}

TEST(MapOperationsHandlerTests, givenAddedMappingWhenFindThenReturnsTrue) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0x2000);
    cl_map_flags flags = CL_MAP_WRITE;
    MemObjSizeArray size = {32, 1, 1};
    MemObjOffsetArray offset = {4, 0, 0};
    handler.add(ptr, 32, flags, size, offset);

    MapInfo outInfo;
    EXPECT_TRUE(handler.find(ptr, outInfo));
    EXPECT_EQ(ptr, outInfo.ptr);
    EXPECT_EQ(32u, outInfo.ptrLength);
    EXPECT_EQ(size, outInfo.size);
    EXPECT_EQ(offset, outInfo.offset);
}

TEST(MapOperationsHandlerTests, givenNonExistentPointerWhenFindThenReturnsFalse) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0x3000);
    void *otherPtr = reinterpret_cast<void *>(0x4000);
    cl_map_flags flags = CL_MAP_WRITE;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    handler.add(ptr, 16, flags, size, offset);

    MapInfo outInfo;
    EXPECT_FALSE(handler.find(otherPtr, outInfo));
}

TEST(MapOperationsHandlerTests, givenAddedMappingWhenRemoveThenSizeBecomesZero) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0x5000);
    cl_map_flags flags = CL_MAP_WRITE;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    handler.add(ptr, 16, flags, size, offset);
    EXPECT_EQ(1u, handler.size());

    handler.remove(ptr);
    EXPECT_EQ(0u, handler.size());
}

TEST(MapOperationsHandlerTests, givenRemoveNonExistentPointerWhenRemoveThenSizeUnchanged) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0x6000);
    void *otherPtr = reinterpret_cast<void *>(0x7000);
    cl_map_flags flags = CL_MAP_WRITE;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    handler.add(ptr, 16, flags, size, offset);

    handler.remove(otherPtr);
    EXPECT_EQ(1u, handler.size());
}

TEST(MapOperationsHandlerTests, givenWriteMappingWhenAddOverlappingWriteThenReturnsFalse) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0x9000);
    cl_map_flags writeFlags = CL_MAP_WRITE;
    MemObjSizeArray size = {64, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    handler.add(ptr, 64, writeFlags, size, offset);

    void *overlappingPtr = reinterpret_cast<void *>(0x9010);
    MemObjSizeArray size2 = {16, 1, 1};
    EXPECT_FALSE(handler.add(overlappingPtr, 16, writeFlags, size2, offset));
}

TEST(MapOperationsHandlerTests, givenWriteMappingsWithNoOverlapWhenAddThenBothSucceed) {
    MapOperationsHandler handler;
    void *ptr1 = reinterpret_cast<void *>(0xA000);
    void *ptr2 = reinterpret_cast<void *>(0xB000);
    cl_map_flags writeFlags = CL_MAP_WRITE;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};

    EXPECT_TRUE(handler.add(ptr1, 16, writeFlags, size, offset));
    EXPECT_TRUE(handler.add(ptr2, 16, writeFlags, size, offset));
    EXPECT_EQ(2u, handler.size());
}

TEST(MapOperationsHandlerTests, givenReadOnlyMappingWhenAddedThenFindReturnsReadOnlyFlag) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0xC000);
    cl_map_flags flags = CL_MAP_READ;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    handler.add(ptr, 16, flags, size, offset);

    MapInfo outInfo;
    handler.find(ptr, outInfo);
    EXPECT_TRUE(outInfo.readOnly);
}

TEST(MapOperationsHandlerTests, givenWriteMappingWhenAddedThenFindReturnsNotReadOnly) {
    MapOperationsHandler handler;
    void *ptr = reinterpret_cast<void *>(0xD000);
    cl_map_flags flags = CL_MAP_WRITE;
    MemObjSizeArray size = {16, 1, 1};
    MemObjOffsetArray offset = {0, 0, 0};
    handler.add(ptr, 16, flags, size, offset);

    MapInfo outInfo;
    handler.find(ptr, outInfo);
    EXPECT_FALSE(outInfo.readOnly);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
