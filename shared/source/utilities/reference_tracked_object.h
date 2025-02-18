/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <atomic>
#include <memory>

namespace NEO {

template <typename CT = int32_t>
class RefCounter {
  public:
    RefCounter()
        : val(0) {
    }

    CT peek() const {
        CT curr = val.load();
        DEBUG_BREAK_IF(curr < 0);
        return curr;
    }

    void inc() {
        [[maybe_unused]] CT curr = ++val;
        DEBUG_BREAK_IF(curr < 1);
    }

    void dec() {
        [[maybe_unused]] CT curr = --val;
        DEBUG_BREAK_IF(curr < 0);
    }

    CT decAndReturnCurrent() {
        return --val;
    }

  protected:
    std::atomic<CT> val;
};

template <typename DerivedClass>
class ReferenceTrackedObject;

template <typename DataType>
class unique_ptr_if_unused : // NOLINT(readability-identifier-naming)
                             public std::unique_ptr<DataType, void (*)(DataType *)> {
    using DeleterFuncType = void (*)(DataType *);

  public:
    unique_ptr_if_unused()
        : std::unique_ptr<DataType, DeleterFuncType>(nullptr, dontDelete) {
    }

    unique_ptr_if_unused(DataType *ptr, bool unused, DeleterFuncType customDeleter = nullptr)
        : std::unique_ptr<DataType, DeleterFuncType>(ptr, unused ? chooseDeleter(ptr, customDeleter) : dontDelete) {
    }

    bool isUnused() const {
        return (this->get_deleter() != dontDelete);
    }

  private:
    static DeleterFuncType chooseDeleter(DataType *inPtr, DeleterFuncType customDeleter) {
        DeleterFuncType deleter = customDeleter;
        if (customDeleter == nullptr) {
            deleter = getObjDeleter(inPtr);
        }

        if (deleter == nullptr) {
            deleter = &doDelete;
        }

        return deleter;
    }

    template <typename DT = DataType>
    static typename std::enable_if_t<std::is_base_of_v<ReferenceTrackedObject<DataType>, DT>, DeleterFuncType> getObjDeleter(DataType *inPtr) {
        if (inPtr != nullptr) {
            return inPtr->getCustomDeleter();
        }
        return nullptr;
    }

    template <typename DT = DataType>
    static typename std::enable_if_t<!std::is_base_of_v<ReferenceTrackedObject<DataType>, DT>, DeleterFuncType> getObjDeleter(DataType *inPtr) {
        return nullptr;
    }

    static void doDelete(DataType *ptr) {
        delete ptr; // NOLINT(clang-analyzer-cplusplus.NewDelete)
    }

    static void dontDelete(DataType *ptr) {
        ;
    }
};

// This class is needed for having both internal and external (api)
// reference counters
// Note : we need both counters because an OCL app can release an OCL object
//        while this object is still needed/valid (e.g. events with callbacks),
//        so we need to have a way of tracking internal usage of these object.
//        At the same time, we can't use one refcount for both internal and external
//        (retain/release api) usage because an OCL application can query object's
//        refcount (this refcount should not be contaminated by our internal usage models)
// Note : internal refcount accumulates also api refcounts (i.e. incrementing/decrementing
//        api refcount will increment/decrement internal refcount as well) - so, object
//        deletion is based on single/atomic decision "if(--internalRefcount == 0)"
template <typename DerivedClass>
class ReferenceTrackedObject {
  public:
    using DeleterFuncType = void (*)(DerivedClass *);

    virtual ~ReferenceTrackedObject();

    int32_t getRefInternalCount() const {
        return refInternal.peek();
    }

    void incRefInternal() {
        refInternal.inc();
    }

    unique_ptr_if_unused<DerivedClass> decRefInternal() {
        auto customDeleter = tryGetCustomDeleter();
        auto current = refInternal.decAndReturnCurrent();
        bool unused = (current == 0);
        UNRECOVERABLE_IF(current < 0);
        return unique_ptr_if_unused<DerivedClass>(static_cast<DerivedClass *>(this), unused, customDeleter);
    }

    int32_t getRefApiCount() const {
        return refApi.peek();
    }

    void incRefApi() {
        refApi.inc();
        refInternal.inc();
    }

    unique_ptr_if_unused<DerivedClass> decRefApi() {
        refApi.dec();
        return decRefInternal();
    }

    DeleterFuncType getCustomDeleter() const {
        return nullptr;
    }

  private:
    DeleterFuncType tryGetCustomDeleter() const {
        const DerivedClass *asDerivedObj = static_cast<const DerivedClass *>(this);
        return asDerivedObj->getCustomDeleter();
    }

    RefCounter<> refInternal;
    RefCounter<> refApi;
};

template <typename DerivedClass>
inline ReferenceTrackedObject<DerivedClass>::~ReferenceTrackedObject() {
    DEBUG_BREAK_IF(refInternal.peek() > 1);
}

template <typename RefTrackedObj>
class DecRefInternalAtScopeEnd final : NEO::NonCopyableAndNonMovableClass {
  public:
    DecRefInternalAtScopeEnd(RefTrackedObj &obj) : object{obj} {
    }

    ~DecRefInternalAtScopeEnd() {
        object.decRefInternal();
    }

  private:
    RefTrackedObj &object;
};

} // namespace NEO
