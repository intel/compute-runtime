/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
#include "runtime/api/dispatch.h"
#include "runtime/utilities/reference_tracked_object.h"
#include "runtime/helpers/abort.h"
#include "runtime/helpers/debug_helpers.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>

namespace OCLRT {

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
    if ((derivedObject->getMagic() & DerivedType::maskMagic) == DerivedType::objectMagic) {
        DEBUG_BREAK_IF(derivedObject->dispatch.icdDispatch != &icdGlobalDispatchTable);
        return derivedObject;
    }

    return nullptr;
}

template <typename DerivedType>
inline DerivedType *castToObjectOrAbort(typename DerivedType::BaseType *object) {
    auto derivedObject = castToObject<DerivedType>(object);
    if (derivedObject == nullptr) {
        abortExecution();
    } else {
        return derivedObject;
    }
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

extern std::thread::id invalidThreadID;

class ConditionVariableWithCounter {
  public:
    ConditionVariableWithCounter() {
        waitersCount = 0;
    }
    template <typename... Args>
    void wait(Args &&... args) {
        ++waitersCount;
        cond.wait(std::forward<Args>(args)...);
        --waitersCount;
    }

    void notify_one() { // NOLINT
        cond.notify_one();
    }

    uint32_t peekNumWaiters() {
        return waitersCount.load();
    }

  private:
    std::atomic_uint waitersCount;
    std::condition_variable cond;
};

template <typename T>
class TakeOwnershipWrapper {
  public:
    TakeOwnershipWrapper(T &obj)
        : obj(obj) {
        this->locked = obj.takeOwnership(true);
    }
    TakeOwnershipWrapper(T &obj, bool lockImmediately)
        : obj(obj) {
        if (lockImmediately) {
            this->locked = obj.takeOwnership(true);
        }
    }
    ~TakeOwnershipWrapper() {
        if (locked) {
            obj.releaseOwnership();
        }
    }
    void unlock() {
        obj.releaseOwnership();
        locked = false;
    }

    void lock() {
        if (!locked)
            this->locked = obj.takeOwnership(true);
    }

  private:
    T &obj;
    bool locked = false;
};

// This class should act as a base class for all CL objects. It will handle the
// MT safe and reference things for every CL object.
template <typename B>
class BaseObject : public B, public ReferenceTrackedObject<DerivedType_t<B>> {
  public:
    typedef BaseObject<B> ThisType;
    typedef B BaseType;
    typedef DerivedType_t<B> DerivedType;

    const static cl_ulong maskMagic = 0xFFFFFFFFFFFFFFFFLL;
    const static cl_ulong deadMagic = 0xFFFFFFFFFFFFFFFFLL;

    BaseObject(const BaseObject &) = delete;
    BaseObject &operator=(const BaseObject &) = delete;

  protected:
    cl_long magic;

    mutable std::mutex mtx;
    mutable ConditionVariableWithCounter cond;
    mutable std::thread::id owner;
    mutable uint32_t recursiveOwnageCounter = 0;

    BaseObject()
        : magic(DerivedType::objectMagic) {
        this->incRefApi();
    }

    ~BaseObject() override {
        magic = deadMagic;
    }

    bool isValid() const {
        return (magic & DerivedType::maskMagic) == DerivedType::objectMagic;
    }

    void convertToInternalObject() {
        this->incRefInternal();
        this->decRefApi();
    }

  public:
    NO_SANITIZE
    cl_ulong getMagic() const {
        return this->magic;
    }

    virtual void retain() {
        DEBUG_BREAK_IF(!isValid());
        this->incRefApi();
    }

    virtual unique_ptr_if_unused<DerivedType> release() {
        DEBUG_BREAK_IF(!isValid());
        return this->decRefApi();
    }

    cl_int getReference() const {
        DEBUG_BREAK_IF(!isValid());
        return this->getRefApiCount();
    }

    MOCKABLE_VIRTUAL bool takeOwnership(bool waitUntilGet) const {
        DEBUG_BREAK_IF(!isValid());

        std::unique_lock<std::mutex> theLock(mtx);
        std::thread::id self = std::this_thread::get_id();

        if (owner == invalidThreadID) {
            owner = self;
            return true;
        }

        if (owner == self) {
            ++recursiveOwnageCounter;
            return true;
        }

        if (waitUntilGet == false) {
            return false;
        }

        cond.wait(theLock, [&] { return owner == invalidThreadID; });
        owner = self;
        recursiveOwnageCounter = 0;
        return true;
    }

    MOCKABLE_VIRTUAL void releaseOwnership() const {
        DEBUG_BREAK_IF(!isValid());

        std::unique_lock<std::mutex> theLock(mtx);

        if (hasOwnership() == false) {
            DEBUG_BREAK_IF(true);
            return;
        }

        if (recursiveOwnageCounter > 0) {
            --recursiveOwnageCounter;
            return;
        }
        owner = invalidThreadID;
        cond.notify_one();
    }

    // checks whether current thread owns object mutex
    bool hasOwnership() const {
        DEBUG_BREAK_IF(!isValid());

        return (owner == std::this_thread::get_id());
    }

    ConditionVariableWithCounter &getCond() {
        return this->cond;
    }

    // Custom allocators for memory tracking CL objects
    static void *operator new(size_t sz);
    static void *operator new(size_t sz, const std::nothrow_t &) noexcept;
    static void operator delete(void *ptr, size_t allocationSize);
    static void operator delete(void *ptr, const std::nothrow_t &)noexcept;
};

// Method called by global factory enabler
template <typename Type>
void populateFactoryTable();

} // namespace OCLRT
