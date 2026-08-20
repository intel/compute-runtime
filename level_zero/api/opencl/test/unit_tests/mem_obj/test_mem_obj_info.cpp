/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct MemObjInfoFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        alignedHostPtr = alignedMalloc(hostPtrSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, alignedHostPtr);
    }

    void TearDown() override {
        for (auto mem : ownedMemObjects) {
            clReleaseMemObject(mem);
        }
        alignedFree(alignedHostPtr);
        Test<LeoCaptureFixture>::TearDown();
    }

    cl_mem newBuffer(size_t size, cl_mem_flags flags = CL_MEM_READ_WRITE, void *hostPtr = nullptr) {
        cl_int errcode = CL_SUCCESS;
        auto mem = clCreateBuffer(clContext, flags, size, hostPtr, &errcode);
        EXPECT_EQ(CL_SUCCESS, errcode);
        EXPECT_NE(nullptr, mem);
        ownedMemObjects.push_back(mem);
        return mem;
    }

    MemObj *asMemObj(cl_mem mem) { return castToObject<MemObj>(mem); }

    static constexpr size_t hostPtrSize = 2 * MemoryConstants::pageSize;

    std::vector<cl_mem> ownedMemObjects;
    void *alignedHostPtr = nullptr;
};

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingTypeThenReturnsBufferObjectType) {
    auto buffer = newBuffer(128u);

    cl_mem_object_type objectType = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_TYPE, sizeof(objectType), &objectType, &retSize));
    EXPECT_EQ(sizeof(cl_mem_object_type), retSize);
    EXPECT_EQ(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER), objectType);
}

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingFlagsThenCreationFlagsAreReturned) {
    const cl_mem_flags creationFlags = CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;
    auto buffer = newBuffer(128u, creationFlags);

    cl_mem_flags flags = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_FLAGS, sizeof(flags), &flags, &retSize));
    EXPECT_EQ(sizeof(cl_mem_flags), retSize);
    EXPECT_EQ(creationFlags, flags);
}

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingSizeThenRequestedSizeIsReturned) {
    constexpr size_t requestedSize = 512u;
    auto buffer = newBuffer(requestedSize);

    size_t size = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_SIZE, sizeof(size), &size, &retSize));
    EXPECT_EQ(sizeof(size_t), retSize);
    EXPECT_EQ(requestedSize, size);
}

TEST_F(MemObjInfoFixture, givenBufferWithoutUseHostPtrWhenQueryingHostPtrThenReturnsNull) {
    auto buffer = newBuffer(128u);

    void *queried = reinterpret_cast<void *>(0x1234);
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_HOST_PTR, sizeof(queried), &queried, nullptr));
    EXPECT_EQ(nullptr, queried);
}

TEST_F(MemObjInfoFixture, givenBufferWithUseHostPtrWhenQueryingHostPtrThenReturnsApplicationPointer) {
    const cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &clDevice->getDevice());
    ASSERT_TRUE(memoryProperties.flags.useHostPtr);

    Buffer buffer{context, memoryProperties, flags, alignedHostPtr, alignedHostPtr, hostPtrSize, true};

    void *queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, buffer.getMemObjectInfo(CL_MEM_HOST_PTR, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(void *), retSize);
    EXPECT_EQ(alignedHostPtr, queried);
}

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingContextThenReturnsOwningContext) {
    auto buffer = newBuffer(128u);

    cl_context queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_CONTEXT, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_context), retSize);
    EXPECT_EQ(clContext, queried);
}

TEST_F(MemObjInfoFixture, givenPlainBufferWhenQueryingSvmUsageThenReturnsFalse) {
    auto buffer = newBuffer(128u);

    cl_bool usesSvm = CL_TRUE;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_USES_SVM_POINTER, sizeof(usesSvm), &usesSvm, nullptr));
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), usesSvm);
}

TEST_F(MemObjInfoFixture, givenBufferMarkedAsSvmWhenQueryingSvmUsageThenReturnsTrue) {
    auto buffer = newBuffer(128u);
    asMemObj(buffer)->setUsesSvm(true);

    cl_bool usesSvm = CL_FALSE;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_USES_SVM_POINTER, sizeof(usesSvm), &usesSvm, nullptr));
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), usesSvm);
    EXPECT_TRUE(asMemObj(buffer)->getUsesSvm());

    asMemObj(buffer)->setUsesSvm(false);
}

TEST_F(MemObjInfoFixture, givenPlainBufferWhenQueryingOffsetAndAssociatedObjectThenBothAreEmpty) {
    auto buffer = newBuffer(128u);

    size_t offset = 0xFFu;
    cl_mem associated = reinterpret_cast<cl_mem>(0x1234);
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_OFFSET, sizeof(offset), &offset, nullptr));
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(associated), &associated, nullptr));
    EXPECT_EQ(0u, offset);
    EXPECT_EQ(nullptr, associated);
    EXPECT_FALSE(asMemObj(buffer)->isSubBuffer());
}

TEST_F(MemObjInfoFixture, givenSubBufferWhenQueryingOffsetAndAssociatedObjectThenParentAndOriginAreReported) {
    constexpr size_t parentSize = 1024u;
    constexpr size_t subOffset = 256u;
    constexpr size_t subSize = 128u;
    auto parent = newBuffer(parentSize);

    cl_buffer_region region{subOffset, subSize};
    cl_int errcode = CL_SUCCESS;
    auto subBuffer = clCreateSubBuffer(parent, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, subBuffer);
    ownedMemObjects.push_back(subBuffer);

    size_t offset = 0;
    cl_mem associated = nullptr;
    size_t size = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(subBuffer)->getMemObjectInfo(CL_MEM_OFFSET, sizeof(offset), &offset, nullptr));
    EXPECT_EQ(CL_SUCCESS, asMemObj(subBuffer)->getMemObjectInfo(CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(associated), &associated, nullptr));
    EXPECT_EQ(CL_SUCCESS, asMemObj(subBuffer)->getMemObjectInfo(CL_MEM_SIZE, sizeof(size), &size, nullptr));

    EXPECT_EQ(subOffset, offset);
    EXPECT_EQ(parent, associated);
    EXPECT_EQ(subSize, size);
    EXPECT_TRUE(asMemObj(subBuffer)->isSubBuffer());
}

TEST_F(MemObjInfoFixture, givenBufferWithoutMappingsWhenQueryingMapCountThenReturnsZero) {
    auto buffer = newBuffer(128u);

    cl_uint mapCount = 0xFFu;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_MAP_COUNT, sizeof(mapCount), &mapCount, &retSize));
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(0u, mapCount);
}

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingReferenceCountThenReturnsOne) {
    auto buffer = newBuffer(128u);

    cl_uint refCount = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_REFERENCE_COUNT, sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(1u, refCount);
}

TEST_F(MemObjInfoFixture, givenRetainedBufferWhenQueryingReferenceCountThenItFollowsRetainCount) {
    auto buffer = newBuffer(128u);
    ASSERT_EQ(CL_SUCCESS, clRetainMemObject(buffer));

    cl_uint refCount = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_REFERENCE_COUNT, sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(2u, refCount);

    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
}

TEST_F(MemObjInfoFixture, givenAllocationHandleParamWhenQueryingThenReturnsInvalidValue) {
    auto buffer = newBuffer(128u);

    size_t retSize = 55u;
    EXPECT_EQ(CL_INVALID_VALUE, asMemObj(buffer)->getMemObjectInfo(CL_MEM_ALLOCATION_HANDLE_INTEL, 0, nullptr, &retSize));
    EXPECT_EQ(55u, retSize);
}

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingCompressionThenBooleanIsReported) {
    auto buffer = newBuffer(128u);

    cl_bool usesCompression = CL_TRUE;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_USES_COMPRESSION_INTEL, sizeof(usesCompression),
                                                             &usesCompression, &retSize));
    EXPECT_EQ(sizeof(cl_bool), retSize);
    EXPECT_EQ(asMemObj(buffer)->isCompressionEnabled() ? static_cast<cl_bool>(CL_TRUE) : static_cast<cl_bool>(CL_FALSE),
              usesCompression);
}

TEST_F(MemObjInfoFixture, givenBufferCreatedWithoutPropertiesWhenQueryingPropertiesThenNothingIsStored) {
    auto buffer = newBuffer(128u);

    size_t retSize = 33u;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_PROPERTIES, 0, nullptr, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(MemObjInfoFixture, givenBufferCreatedWithPropertiesWhenQueryingPropertiesThenStoredArrayIsNullTerminated) {
    cl_mem_properties properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, 0};
    cl_int errcode = CL_SUCCESS;
    auto buffer = clCreateBufferWithProperties(clContext, properties, 0, 128u, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, buffer);
    ownedMemObjects.push_back(buffer);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_PROPERTIES, 0, nullptr, &retSize));
    ASSERT_EQ(3u * sizeof(cl_mem_properties), retSize);

    std::vector<cl_mem_properties> stored(retSize / sizeof(cl_mem_properties));
    ASSERT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_MEM_PROPERTIES, retSize, stored.data(), nullptr));
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_MEM_FLAGS), stored[0]);
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_MEM_READ_ONLY), stored[1]);
    EXPECT_EQ(0u, stored[2]);
}

TEST_F(MemObjInfoFixture, givenBufferWhenQueryingL0HandleThenUsmPointerIsReturned) {
    auto buffer = newBuffer(128u);
    auto pBuffer = castToObject<Buffer>(buffer);
    ASSERT_NE(nullptr, pBuffer);

    void *queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(CL_L0_MEM_OBJ_HANDLE, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(void *), retSize);
    EXPECT_EQ(pBuffer->getUsmPtr(), queried);
}

TEST_F(MemObjInfoFixture, givenUnknownParamWhenQueryingMemObjectInfoThenReturnsInvalidValue) {
    auto buffer = newBuffer(128u);

    size_t retSize = 21u;
    EXPECT_EQ(CL_INVALID_VALUE, asMemObj(buffer)->getMemObjectInfo(0xDEAD0000u, 0, nullptr, &retSize));
    EXPECT_EQ(21u, retSize);
}

TEST_F(MemObjInfoFixture, givenTooSmallBufferWhenQueryingMemObjectInfoThenReturnsInvalidValue) {
    auto buffer = newBuffer(128u);

    cl_mem_flags flags = 0;
    EXPECT_EQ(CL_INVALID_VALUE, asMemObj(buffer)->getMemObjectInfo(CL_MEM_FLAGS, sizeof(cl_mem_flags) - 1, &flags, nullptr));
}

TEST_F(MemObjInfoFixture, givenSizeOnlyQueryWhenQueryingScalarParamsThenSizesAreReported) {
    auto buffer = newBuffer(128u);

    const std::pair<cl_mem_info, size_t> scalarParams[] = {
        {CL_MEM_TYPE, sizeof(cl_mem_object_type)},
        {CL_MEM_FLAGS, sizeof(cl_mem_flags)},
        {CL_MEM_SIZE, sizeof(size_t)},
        {CL_MEM_HOST_PTR, sizeof(void *)},
        {CL_MEM_CONTEXT, sizeof(cl_context)},
        {CL_MEM_USES_SVM_POINTER, sizeof(cl_bool)},
        {CL_MEM_OFFSET, sizeof(size_t)},
        {CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(cl_mem)},
        {CL_MEM_MAP_COUNT, sizeof(cl_uint)},
        {CL_MEM_REFERENCE_COUNT, sizeof(cl_uint)},
        {CL_MEM_USES_COMPRESSION_INTEL, sizeof(cl_bool)}};

    for (const auto &[paramName, expectedSize] : scalarParams) {
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, asMemObj(buffer)->getMemObjectInfo(paramName, 0, nullptr, &retSize))
            << "param 0x" << std::hex << paramName;
        EXPECT_EQ(expectedSize, retSize) << "param 0x" << std::hex << paramName;
    }
}

TEST_F(MemObjInfoFixture, givenUnrestrictedBufferWhenCheckingAccessFlagsThenNothingIsInvalid) {
    auto memObj = asMemObj(newBuffer(128u));

    EXPECT_FALSE(memObj->readMemObjFlagsInvalid());
    EXPECT_FALSE(memObj->writeMemObjFlagsInvalid());
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_READ));
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE));
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE_INVALIDATE_REGION));
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_READ | CL_MAP_WRITE));
}

TEST_F(MemObjInfoFixture, givenHostReadOnlyBufferWhenCheckingAccessFlagsThenOnlyWriteSideIsInvalid) {
    auto memObj = asMemObj(newBuffer(128u, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY));

    EXPECT_FALSE(memObj->readMemObjFlagsInvalid());
    EXPECT_TRUE(memObj->writeMemObjFlagsInvalid());
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_READ));
    EXPECT_TRUE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE));
    EXPECT_TRUE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE_INVALIDATE_REGION));
}

TEST_F(MemObjInfoFixture, givenHostWriteOnlyBufferWhenCheckingAccessFlagsThenOnlyReadSideIsInvalid) {
    auto memObj = asMemObj(newBuffer(128u, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY));

    EXPECT_TRUE(memObj->readMemObjFlagsInvalid());
    EXPECT_FALSE(memObj->writeMemObjFlagsInvalid());
    EXPECT_TRUE(memObj->mapMemObjFlagsInvalid(CL_MAP_READ));
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE));
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE_INVALIDATE_REGION));
}

TEST_F(MemObjInfoFixture, givenHostNoAccessBufferWhenCheckingAccessFlagsThenEverythingIsInvalid) {
    auto memObj = asMemObj(newBuffer(128u, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS));

    EXPECT_TRUE(memObj->readMemObjFlagsInvalid());
    EXPECT_TRUE(memObj->writeMemObjFlagsInvalid());
    EXPECT_TRUE(memObj->mapMemObjFlagsInvalid(CL_MAP_READ));
    EXPECT_TRUE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE));
    EXPECT_TRUE(memObj->mapMemObjFlagsInvalid(CL_MAP_WRITE_INVALIDATE_REGION));
}

TEST_F(MemObjInfoFixture, givenRestrictedBufferWhenMapFlagsAreEmptyThenNothingIsInvalid) {
    auto memObj = asMemObj(newBuffer(128u, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS));
    EXPECT_FALSE(memObj->mapMemObjFlagsInvalid(0));
}

struct MemObjCallbackRecorder {
    static constexpr size_t capacity = 8u;
    static std::array<int, capacity> invocations;
    static size_t count;

    static void reset() {
        invocations.fill(0);
        count = 0u;
    }

    template <int id>
    static void CL_CALLBACK callback(cl_mem, void *userData) {
        if (count < capacity) {
            invocations[count++] = id;
        }
        if (userData != nullptr) {
            *static_cast<int *>(userData) = id;
        }
    }
};

std::array<int, MemObjCallbackRecorder::capacity> MemObjCallbackRecorder::invocations{};
size_t MemObjCallbackRecorder::count = 0u;

TEST_F(MemObjInfoFixture, givenRegisteredDestructorCallbacksWhenBufferIsReleasedThenTheyRunInReverseOrder) {
    MemObjCallbackRecorder::reset();

    cl_int errcode = CL_SUCCESS;
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 128u, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    asMemObj(buffer)->addCallback(&MemObjCallbackRecorder::callback<1>, nullptr);
    asMemObj(buffer)->addCallback(&MemObjCallbackRecorder::callback<2>, nullptr);
    asMemObj(buffer)->addCallback(&MemObjCallbackRecorder::callback<3>, nullptr);

    EXPECT_EQ(0u, MemObjCallbackRecorder::count);

    ASSERT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));

    ASSERT_EQ(3u, MemObjCallbackRecorder::count);
    EXPECT_EQ(3, MemObjCallbackRecorder::invocations[0]);
    EXPECT_EQ(2, MemObjCallbackRecorder::invocations[1]);
    EXPECT_EQ(1, MemObjCallbackRecorder::invocations[2]);
}

TEST_F(MemObjInfoFixture, givenRegisteredDestructorCallbackWithUserDataWhenBufferIsReleasedThenUserDataIsForwarded) {
    MemObjCallbackRecorder::reset();
    int userData = 0;

    cl_int errcode = CL_SUCCESS;
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 128u, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);

    asMemObj(buffer)->addCallback(&MemObjCallbackRecorder::callback<9>, &userData);
    ASSERT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));

    EXPECT_EQ(9, userData);
}

TEST_F(MemObjInfoFixture, givenRetainedBufferWithCallbackWhenReleasedOnceThenCallbackDoesNotRunYet) {
    MemObjCallbackRecorder::reset();

    cl_int errcode = CL_SUCCESS;
    auto buffer = clCreateBuffer(clContext, CL_MEM_READ_WRITE, 128u, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    asMemObj(buffer)->addCallback(&MemObjCallbackRecorder::callback<4>, nullptr);

    ASSERT_EQ(CL_SUCCESS, clRetainMemObject(buffer));
    ASSERT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
    EXPECT_EQ(0u, MemObjCallbackRecorder::count);

    ASSERT_EQ(CL_SUCCESS, clReleaseMemObject(buffer));
    EXPECT_EQ(1u, MemObjCallbackRecorder::count);
}

TEST_F(MemObjInfoFixture, givenBufferWhenStoringPropertiesThenTrailingZeroTerminatesTheVector) {
    auto memObj = asMemObj(newBuffer(128u));

    cl_mem_properties properties[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0};
    memObj->storeProperties(properties);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, memObj->getMemObjectInfo(CL_MEM_PROPERTIES, 0, nullptr, &retSize));
    ASSERT_EQ(5u * sizeof(cl_mem_properties), retSize);

    std::vector<cl_mem_properties> stored(retSize / sizeof(cl_mem_properties));
    ASSERT_EQ(CL_SUCCESS, memObj->getMemObjectInfo(CL_MEM_PROPERTIES, retSize, stored.data(), nullptr));
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_MEM_FLAGS), stored[0]);
    EXPECT_EQ(static_cast<cl_mem_properties>(CL_MEM_FLAGS_INTEL), stored[2]);
    EXPECT_EQ(0u, stored[4]);
}

TEST_F(MemObjInfoFixture, givenNullPropertiesWhenStoringThenNothingIsAppended) {
    auto memObj = asMemObj(newBuffer(128u));
    memObj->storeProperties(nullptr);

    size_t retSize = 7u;
    EXPECT_EQ(CL_SUCCESS, memObj->getMemObjectInfo(CL_MEM_PROPERTIES, 0, nullptr, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(MemObjInfoFixture, givenBufferWhenInspectingTypePredicatesThenBufferSideIsReported) {
    auto memObj = asMemObj(newBuffer(128u));

    EXPECT_TRUE(memObj->isBuffer());
    EXPECT_FALSE(memObj->isImage());
    EXPECT_EQ(clContext, static_cast<cl_context>(memObj->getContext()));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
