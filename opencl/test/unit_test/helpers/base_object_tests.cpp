/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/api/api.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/program/program.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

namespace NEO {
typedef struct _cl_object_for_test2 *cl_object_for_test2;

struct _cl_object_for_test2 : public ClDispatch {
};

template <>
struct OpenCLObjectMapper<_cl_object_for_test2> {
    typedef struct ObjectForTest2 DerivedType;
};

template <>
struct OpenCLObjectMapper<ObjectForTest2> {
    typedef _cl_object_for_test2 BaseType;
};

struct ObjectForTest2 : public NEO::BaseObject<_cl_object_for_test2> {
    static const cl_ulong objectMagic = 0x13650a12b79ce4dfLL;
};

template <typename TypeParam>
struct BaseObjectTests : public ::testing::Test {
};

template <typename OclObject>
class MockObjectBase : public OclObject {
  public:
    using OclObject::OclObject;

    void setInvalidMagic() {
        validMagic = this->magic;
        this->magic = 0x0101010101010101LL;
    }
    void setInvalidIcdDispath() {
        this->dispatch.icdDispatch = reinterpret_cast<SDispatchTable *>(this);
    }
    void setValidMagic() {
        this->magic = validMagic;
    }

    bool isObjectValid() const {
        return this->isValid();
    }

    cl_ulong validMagic;
};

template <typename BaseType>
class MockObject : public MockObjectBase<BaseType> {};

template <>
class MockObject<Buffer> : public MockObjectBase<Buffer> {
  public:
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) override {}
};

template <>
class MockObject<Program> : public MockObjectBase<Program> {
  public:
    MockObject() : MockObjectBase<Program>(nullptr, false, toClDeviceVector(*(new MockClDevice(new MockDevice())))), device(this->clDevices[0]) {}
    ~MockObject() override {
        Program::clDevices.clear();
    }

  private:
    std::unique_ptr<ClDevice> device;
};

typedef ::testing::Types<
    MockPlatform,
    IntelAccelerator,
    // Context,
    // Program,
    // Kernel,
    // Sampler
    // others...
    MockCommandQueue>
    BaseObjectTypes;

typedef ::testing::Types<
    MockPlatform,
    IntelAccelerator,
    Context,
    Program,
    Buffer,
    MockCommandQueue>
    BaseObjectTypesForCastInvalidMagicTest;

TYPED_TEST_SUITE(BaseObjectTests, BaseObjectTypes);

// "typedef" BaseObjectTests template to use with different TypeParams for testing
template <typename T>
using BaseObjectWithDefaultCtorTests = BaseObjectTests<T>;
TYPED_TEST_SUITE(BaseObjectWithDefaultCtorTests, BaseObjectTypesForCastInvalidMagicTest);

TYPED_TEST(BaseObjectWithDefaultCtorTests, GivenInvalidMagicWhenCastingToObjectThenNullptrIsReturned) {
    MockObject<TypeParam> *object = new MockObject<TypeParam>;
    EXPECT_TRUE(object->isObjectValid());
    object->setInvalidMagic();
    EXPECT_FALSE(object->isObjectValid());

    auto objectCasted = castToObject<TypeParam>(object);
    EXPECT_EQ(nullptr, objectCasted);

    object->setValidMagic();
    delete object;
}

TYPED_TEST(BaseObjectWithDefaultCtorTests, whenCastToObjectWithInvalidIcdDispatchThenReturnsNullptr) {
    auto object = std::make_unique<MockObject<TypeParam>>();
    object->setInvalidIcdDispath();

    auto objectCasted = castToObject<TypeParam>(object.get());
    EXPECT_EQ(nullptr, objectCasted);
}

TYPED_TEST(BaseObjectTests, WhenRetainingAndReleasingThenObjectReferenceIsUpdated) {
    TypeParam *object = new TypeParam;
    object->retain();
    EXPECT_EQ(2, object->getReference());

    object->release();
    EXPECT_EQ(1, object->getReference()); // NOLINT(clang-analyzer-cplusplus.NewDelete)

    object->release();

    // MemoryLeakListener will detect a leak
    // if release doesn't delete memory.
}

TYPED_TEST(BaseObjectTests, GivenNullWhenCastingToObjectThenNullptrIsReturned) {
    typename TypeParam::BaseType *handle = nullptr;
    auto object = castToObject<TypeParam>(handle);
    EXPECT_EQ(nullptr, object);
}

TYPED_TEST(BaseObjectTests, WhenCastingToFromBaseTypeThenCorrectTypeIsDerived) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto objectNew = castToObject<TypeParam>(baseObject);
    EXPECT_EQ(&object, objectNew);
}

TYPED_TEST(BaseObjectTests, WhenCastingToSameTypeThenSameObjectIsReturned) {
    TypeParam object;

    auto objectNew = castToObject<TypeParam>(&object);
    EXPECT_EQ(&object, objectNew);
}

TYPED_TEST(BaseObjectTests, WhenCastingToDifferentTypeThenNullptrIsReturned) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto notOriginalType = reinterpret_cast<ObjectForTest2::BaseType *>(baseObject);
    auto invalidObject = castToObject<ObjectForTest2>(notOriginalType);
    EXPECT_EQ(nullptr, invalidObject);
}

TYPED_TEST(BaseObjectTests, WhenCastingToDispatchTableThenEntriesAreCorrect) {
    TypeParam objectDrv;

    // Automatic downcasting to _cl_type *.
    typename TypeParam::BaseType *objectCL = &objectDrv;
    sharingFactory.fillGlobalDispatchTable();

    // Common runtime casts to generic type assuming
    // the dispatch table is the first ptr in the structure
    auto genericObject = reinterpret_cast<ClDispatch *>(objectCL);
    EXPECT_EQ(globalDispatchTable.icdDispatch, genericObject->dispatch.icdDispatch);
    EXPECT_EQ(globalDispatchTable.crtDispatch, genericObject->dispatch.crtDispatch);

    EXPECT_EQ(reinterpret_cast<void *>(clGetKernelArgInfo), genericObject->dispatch.crtDispatch->clGetKernelArgInfo);

    EXPECT_EQ(reinterpret_cast<void *>(clGetImageParamsINTEL), genericObject->dispatch.crtDispatch->clGetImageParamsINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clCreateAcceleratorINTEL), genericObject->dispatch.crtDispatch->clCreateAcceleratorINTEL);
    EXPECT_EQ(reinterpret_cast<void *>(clGetAcceleratorInfoINTEL), genericObject->dispatch.crtDispatch->clGetAcceleratorInfoINTEL);
    EXPECT_EQ(reinterpret_cast<void *>(clRetainAcceleratorINTEL), genericObject->dispatch.crtDispatch->clRetainAcceleratorINTEL);
    EXPECT_EQ(reinterpret_cast<void *>(clReleaseAcceleratorINTEL), genericObject->dispatch.crtDispatch->clReleaseAcceleratorINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clCreatePerfCountersCommandQueueINTEL),
              genericObject->dispatch.crtDispatch->clCreatePerfCountersCommandQueueINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clSetPerformanceConfigurationINTEL),
              genericObject->dispatch.crtDispatch->clSetPerformanceConfigurationINTEL);

    // Check empty placeholder dispatch table entries are null
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder12);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder13);

    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder18);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder19);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder20);
    EXPECT_EQ(nullptr, genericObject->dispatch.crtDispatch->placeholder21);
}

TYPED_TEST(BaseObjectTests, WhenTakingAndReleasingOwnershipThenOwnershipCountIsUpdated) {
    TypeParam obj;
    EXPECT_FALSE(obj.hasOwnership());

    obj.takeOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.takeOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.takeOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.releaseOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.releaseOwnership();
    EXPECT_TRUE(obj.hasOwnership());

    obj.releaseOwnership();
    EXPECT_FALSE(obj.hasOwnership());
}

TEST(CastToBuffer, WhenCastingFromMemObjThenBehavesAsExpected) {
    MockContext context;
    auto buffer = BufferHelper<>::create(&context);
    MemObj *memObj = buffer;
    cl_mem clObj = buffer;

    EXPECT_EQ(buffer, castToObject<Buffer>(clObj));
    EXPECT_EQ(memObj, castToObject<MemObj>(clObj));
    EXPECT_EQ(nullptr, castToObject<Image>(clObj));

    buffer->release();
}

TEST(CastToImage, WhenCastingFromMemObjThenBehavesAsExpected) {
    MockContext context;
    auto image = Image2dHelperUlt<>::create(&context);
    MemObj *memObj = image;
    cl_mem clObj = image;

    EXPECT_EQ(image, castToObject<Image>(clObj));
    EXPECT_EQ(memObj, castToObject<MemObj>(clObj));
    EXPECT_EQ(nullptr, castToObject<Buffer>(clObj));

    image->release();
}

extern std::thread::id tempThreadID;
class MockBuffer : public MockBufferStorage, public Buffer {
    using MockBufferStorage::device;

  public:
    MockBuffer() : MockBufferStorage(),
                   Buffer(nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                          CL_MEM_USE_HOST_PTR, 0, sizeof(data), &data, &data, GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) override {
    }
};

TEST(BaseObjectTest, WhenUsingOwnershipWrapperThenOwnershipIsSetCorrectly) {
    MockBuffer buffer;
    {
        TakeOwnershipWrapper<Buffer> bufferOwnership(buffer, false);
        EXPECT_FALSE(buffer.hasOwnership());
    }
    {
        TakeOwnershipWrapper<Buffer> bufferOwnership(buffer, true);
        EXPECT_TRUE(buffer.hasOwnership());
        bufferOwnership.unlock();
        EXPECT_FALSE(buffer.hasOwnership());
    }
}

TYPED_TEST(BaseObjectTests, WhenObjectIsCreatedThenNumWaitersIsZero) {
    TypeParam *object = new TypeParam;

    EXPECT_EQ(0U, object->getCond().peekNumWaiters());
    object->release();
}

TYPED_TEST(BaseObjectTests, WhenConvertingToInternalObjectThenRefApiCountIsSetToZero) {
    class ObjectForTest : public NEO::MemObj {
      public:
        ObjectForTest() : MemObj(nullptr, 0, {}, 0, 0, 0u, nullptr, nullptr, 0, false, false, false) {
        }

        void convertToInternalObject(void) {
            NEO::BaseObject<_cl_mem>::convertToInternalObject();
        }
    };
    ObjectForTest *object = new ObjectForTest;
    EXPECT_EQ(1, object->getRefApiCount());
    EXPECT_EQ(1, object->getRefInternalCount());
    object->convertToInternalObject();
    EXPECT_EQ(0, object->getRefApiCount()); // NOLINT(clang-analyzer-cplusplus.NewDelete)
    EXPECT_EQ(1, object->getRefInternalCount());
    object->decRefInternal();
}

TYPED_TEST(BaseObjectTests, GivenNullptrWhenCastingToObjectOrAbortThenExceptionIsThrown) {
    EXPECT_ANY_THROW(castToObjectOrAbort<TypeParam>(nullptr));
}

TYPED_TEST(BaseObjectTests, WhenCastingToObjectOrAbortThenCorrectTypeIsDerived) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto objectNew = castToObjectOrAbort<TypeParam>(baseObject);
    EXPECT_EQ(&object, objectNew);
}

TYPED_TEST(BaseObjectTests, WhenCastToOrAbortWithDifferentTypeThenExceptionIsThrown) {
    TypeParam object;
    typename TypeParam::BaseType *baseObject = &object;

    auto notOriginalType = reinterpret_cast<ObjectForTest2::BaseType *>(baseObject);
    EXPECT_ANY_THROW(castToObjectOrAbort<ObjectForTest2>(notOriginalType));
}
} // namespace NEO
