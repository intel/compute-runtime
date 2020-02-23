/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"

#include <atomic>
#include <memory>
#include <type_traits>

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
        CT curr = ++val;
        DEBUG_BREAK_IF(curr < 1);
        UNUSED_VARIABLE(curr);
    }

    bool dec() {
        CT curr = --val;
        DEBUG_BREAK_IF(curr < 0);
        return (curr == 0);
    }

    CT decAndReturnCurrent() {
        return --val;
    }

    bool peekIsZero() const {
        return (val == 0);
    }

  protected:
    std::atomic<CT> val;
};

template <typename DerivedClass>
class ReferenceTrackedObject;

template <typename DataType>
class unique_ptr_if_unused : public std::unique_ptr<DataType, void (*)(DataType *)> {
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
    static typename std::enable_if<std::is_base_of<ReferenceTrackedObject<DataType>, DT>::value, DeleterFuncType>::type getObjDeleter(DataType *inPtr) {
        if (inPtr != nullptr) {
            return inPtr->getCustomDeleter();
        }
        return nullptr;
    }

    template <typename DT = DataType>
    static typename std::enable_if<!std::is_base_of<ReferenceTrackedObject<DataType>, DT>::value, DeleterFuncType>::type getObjDeleter(DataType *inPtr) {
        return nullptr;
    }

    static void doDelete(DataType *ptr) {
        delete ptr;
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

    using DeleterFuncType = void (*)(DerivedClass *);
    DeleterFuncType getCustomDeleter() const {
        return nullptr;
    }

    bool peekHasZeroRefcounts() const {
        return refInternal.peekIsZero();
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
} // namespace NEO
