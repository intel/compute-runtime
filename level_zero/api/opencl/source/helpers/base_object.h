/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/abort.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include "level_zero/api/opencl/source/api/dispatch.h"

#include <condition_variable>
#include <mutex>
#include <thread>

struct _cl_mem;

namespace NEO {
namespace LEO {

#if defined(__clang__)
#define NO_SANITIZE __attribute__((no_sanitize("undefined")))
#else
#define NO_SANITIZE
#endif
template <typename Type>
struct OpenCLObjectMapper {
};

template <typename T>
using DerivedType_t = typename OpenCLObjectMapper<T>::DerivedType;

template <typename DerivedType>
NO_SANITIZE inline DerivedType *castToObject(typename DerivedType::BaseType *object) {
    if (object == nullptr) {
        return nullptr;
    }

    auto derivedObject = static_cast<DerivedType *>(object);
    if (((derivedObject->getMagic() & DerivedType::maskMagic) == DerivedType::objectMagic) &&
        (derivedObject->dispatch.icdDispatch == &icdGlobalDispatchTable)) {
        return derivedObject;
    }

    return nullptr;
}

template <typename DerivedType>
inline const DerivedType *castToObject(const typename DerivedType::BaseType *object) {
    return castToObject<DerivedType>(const_cast<typename DerivedType::BaseType *>(object));
}

template <typename DerivedType>
inline DerivedType *castToObject(const void *object) {
    cl_mem clMem = const_cast<cl_mem>(static_cast<const _cl_mem *>(object));
    return castToObject<DerivedType>(clMem);
}

// This class should act as a base class for all CL objects. It will handle the
// MT safe and reference things for every CL object.
template <typename B>
class BaseObject : public B, public ReferenceTrackedObject<DerivedType_t<B>>, NEO::NonCopyableAndNonMovableClass {
  public:
    typedef BaseObject<B> ThisType;
    typedef B BaseType;
    typedef DerivedType_t<B> DerivedType;

    constexpr static cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    constexpr static cl_ulong deadMagic = maskMagic;

  protected:
    cl_long magic;

    std::recursive_mutex mtx;

    BaseObject()
        : magic(DerivedType::objectMagic) {
        this->incRefApi();
    }

    ~BaseObject() override {
        magic = deadMagic;
    }

  public:
    NO_SANITIZE
    cl_ulong getMagic() const {
        return this->magic;
    }

    virtual void retain() {
        this->incRefApi();
    }

    virtual unique_ptr_if_unused<DerivedType> release() {
        return this->decRefApi();
    }

    cl_int getReference() const {
        return this->getRefApiCount();
    }

    [[nodiscard]] std::unique_lock<std::recursive_mutex> takeOwnership() {
        return std::unique_lock(this->mtx);
    }
};

} // namespace LEO
} // namespace NEO
