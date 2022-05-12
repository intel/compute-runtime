/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

thread_local ze_bool_t tracingInProgress = 0;

struct APITracerContextImp globalAPITracerContextImp;
struct APITracerContextImp *pGlobalAPITracerContextImp = &globalAPITracerContextImp;

APITracer *APITracer::create() {
    APITracerImp *tracer = new APITracerImp;
    tracer->tracingState = disabledState;
    tracer->tracerFunctions = {};
    UNRECOVERABLE_IF(tracer == nullptr);
    return tracer;
}

ze_result_t createAPITracer(zet_context_handle_t hContext, const zet_tracer_exp_desc_t *desc, zet_tracer_exp_handle_t *phTracer) {

    if (!pGlobalAPITracerContextImp->isTracingEnabled()) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    APITracerImp *tracer = static_cast<APITracerImp *>(APITracer::create());

    tracer->tracerFunctions.pUserData = desc->pUserData;

    *phTracer = tracer->toHandle();
    return ZE_RESULT_SUCCESS;
}

// This destructor will be called only during at-exit processing,
// Hence, this function is executing in a single threaded environment,
// and requires no mutex.
APITracerContextImp::~APITracerContextImp() {
    std::list<ThreadPrivateTracerData *>::iterator itr = threadTracerDataList.begin();
    while (itr != threadTracerDataList.end()) {
        (*itr)->clearThreadTracerDataOnList();
        itr = threadTracerDataList.erase(itr);
    }
}

ze_result_t APITracerImp::destroyTracer(zet_tracer_exp_handle_t phTracer) {

    APITracerImp *tracer = static_cast<APITracerImp *>(phTracer);

    ze_result_t result = pGlobalAPITracerContextImp->finalizeDisableImpTracingWait(tracer);
    if (result == ZE_RESULT_SUCCESS) {
        delete L0::APITracer::fromHandle(phTracer);
    }
    return result;
}

ze_result_t APITracerImp::setPrologues(zet_core_callbacks_t *pCoreCbs) {

    if (this->tracingState != disabledState) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    this->tracerFunctions.corePrologues = *pCoreCbs;

    return ZE_RESULT_SUCCESS;
}

ze_result_t APITracerImp::setEpilogues(zet_core_callbacks_t *pCoreCbs) {

    if (this->tracingState != disabledState) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    this->tracerFunctions.coreEpilogues = *pCoreCbs;

    return ZE_RESULT_SUCCESS;
}

ze_result_t APITracerImp::enableTracer(ze_bool_t enable) {
    return pGlobalAPITracerContextImp->enableTracingImp(this, enable);
}

void APITracerContextImp::addThreadTracerDataToList(ThreadPrivateTracerData *threadDataP) {
    std::lock_guard<std::mutex> lock(threadTracerDataListMutex);
    threadTracerDataList.push_back(threadDataP);
}

void APITracerContextImp::removeThreadTracerDataFromList(ThreadPrivateTracerData *threadDataP) {
    std::lock_guard<std::mutex> lock(threadTracerDataListMutex);
    if (threadTracerDataList.empty())
        return;
    threadTracerDataList.remove(threadDataP);
}

thread_local ThreadPrivateTracerData myThreadPrivateTracerData;

ThreadPrivateTracerData::ThreadPrivateTracerData() {
    isInitialized = false;
    onList = false;
    tracerArrayPointer.store(nullptr, std::memory_order_relaxed);
}

ThreadPrivateTracerData::~ThreadPrivateTracerData() {
    if (onList) {
        globalAPITracerContextImp.removeThreadTracerDataFromList(this);
        onList = false;
    }
    tracerArrayPointer.store(nullptr, std::memory_order_relaxed);
}

void ThreadPrivateTracerData::removeThreadTracerDataFromList(void) {
    if (onList) {
        globalAPITracerContextImp.removeThreadTracerDataFromList(this);
        onList = false;
    }
    tracerArrayPointer.store(nullptr, std::memory_order_relaxed);
}

bool ThreadPrivateTracerData::testAndSetThreadTracerDataInitializedAndOnList(void) {
    if (!isInitialized) {
        isInitialized = true;
        onList = true;
        globalAPITracerContextImp.addThreadTracerDataToList(&myThreadPrivateTracerData);
    }
    return onList;
}

bool APITracerContextImp::isTracingEnabled() { return driver_ddiTable.enableTracing; }

//
// Walk the list of per-thread private data structures, testing
// whether any of them reference this array.
//
// Return 1 if a reference is found.  Otherwise return 0.
//
ze_bool_t APITracerContextImp::testForTracerArrayReferences(tracer_array_t *tracerArray) {
    std::lock_guard<std::mutex> lock(threadTracerDataListMutex);
    std::list<ThreadPrivateTracerData *>::iterator itr;
    for (itr = threadTracerDataList.begin();
         itr != threadTracerDataList.end();
         itr++) {
        if ((*itr)->tracerArrayPointer.load(std::memory_order_relaxed) == tracerArray)
            return 1;
    }
    return 0;
}

//
// Walk the retiring_tracer_array_list, checking each member of the list for
// references by per thread tracer array pointer. Delete and free
// each tracer array that has no per-thread references.
//
// Return the number of entries on the retiring tracer array list.
//
size_t APITracerContextImp::testAndFreeRetiredTracers() {
    std::list<tracer_array_t *>::iterator itr = this->retiringTracerArrayList.begin();
    while (itr != this->retiringTracerArrayList.end()) {
        tracer_array_t *retiringTracerArray = *itr;
        itr++;
        if (testForTracerArrayReferences(retiringTracerArray))
            continue;
        this->retiringTracerArrayList.remove(retiringTracerArray);
        delete[] retiringTracerArray->tracerArrayEntries;
        delete retiringTracerArray;
    }
    return this->retiringTracerArrayList.size();
}

size_t APITracerContextImp::updateTracerArrays() {
    tracer_array_t *newTracerArray;
    size_t newTracerArrayCount = this->enabledTracerImpList.size();

    if (newTracerArrayCount != 0) {

        newTracerArray = new tracer_array_t;

        newTracerArray->tracerArrayCount = newTracerArrayCount;
        newTracerArray->tracerArrayEntries = new tracer_array_entry_t[newTracerArrayCount];
        //
        // iterate over the list of enabled tracers, copying their entries into the
        // new tracer array
        //
        size_t i = 0;
        std::list<struct APITracerImp *>::iterator itr;
        for (itr = enabledTracerImpList.begin(); itr != enabledTracerImpList.end(); itr++) {
            newTracerArray->tracerArrayEntries[i] = (*itr)->tracerFunctions;
            i++;
        }

    } else {
        newTracerArray = &emptyTracerArray;
    }
    //
    // active_tracer_array.load can use memory_order_relaxed here because
    // there is logically no transfer of other memory context between
    // threads in this case.
    //
    tracer_array_t *activeTracerArrayShadow = activeTracerArray.load(std::memory_order_relaxed);
    if (activeTracerArrayShadow != &emptyTracerArray) {
        retiringTracerArrayList.push_back(activeTracerArrayShadow);
    }
    //
    // This active_tracer_array.store must use memory_order_release.
    // This store DOES signal a logical transfer of tracer state information
    // from this thread to the tracing threads.
    //
    activeTracerArray.store(newTracerArray, std::memory_order_release);
    return testAndFreeRetiredTracers();
}

ze_result_t APITracerContextImp::enableTracingImp(struct APITracerImp *tracerImp, ze_bool_t enable) {
    std::lock_guard<std::mutex> lock(traceTableMutex);
    ze_result_t result;
    switch (tracerImp->tracingState) {
    case disabledState:
        if (enable) {
            enabledTracerImpList.push_back(tracerImp);
            tracerImp->tracingState = enabledState;
            updateTracerArrays();
        }
        result = ZE_RESULT_SUCCESS;
        break;

    case enabledState:
        if (!enable) {
            enabledTracerImpList.remove(tracerImp);
            tracerImp->tracingState = disabledWaitingState;
            if (updateTracerArrays() == 0)
                tracerImp->tracingState = disabledState;
        }
        result = ZE_RESULT_SUCCESS;
        break;

    case disabledWaitingState:
        result = ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
        break;

    default:
        result = ZE_RESULT_ERROR_UNINITIALIZED;
        UNRECOVERABLE_IF(true);
        break;
    }
    return result;
}

// This is called by the destroy tracer method.
//
// This routine will return ZE_RESULT_SUCCESS
// state if either it has never been enabled,
// or if it has been enabled and then disabled.
//
// On ZE_RESULT_SUCESS, the destroy tracer method
// can free the tracer's memory.
//
// ZE_RESULT_ERROR_UNINITIALIZED is returned
// if the tracer has been enabled but not
// disabled.  The destroy tracer method
// should NOT free this tracer's memory.
//
ze_result_t APITracerContextImp::finalizeDisableImpTracingWait(struct APITracerImp *tracerImp) {
    std::lock_guard<std::mutex> lock(traceTableMutex);
    ze_result_t result;
    switch (tracerImp->tracingState) {
    case disabledState:
        result = ZE_RESULT_SUCCESS;
        break;

    case enabledState:
        result = ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
        break;

    case disabledWaitingState:
        while (testAndFreeRetiredTracers() != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        tracerImp->tracingState = disabledState;
        result = ZE_RESULT_SUCCESS;
        break;

    default:
        result = ZE_RESULT_ERROR_UNINITIALIZED;
        UNRECOVERABLE_IF(true);
        break;
    }

    return result;
}

void *APITracerContextImp::getActiveTracersList() {
    tracer_array_t *stableTracerArray = nullptr;

    if (!myThreadPrivateTracerData.testAndSetThreadTracerDataInitializedAndOnList()) {
        return nullptr;
    }

    do {
        stableTracerArray = pGlobalAPITracerContextImp->activeTracerArray.load(std::memory_order_acquire);
        myThreadPrivateTracerData.tracerArrayPointer.store(stableTracerArray, std::memory_order_relaxed);
    } while (stableTracerArray !=
             pGlobalAPITracerContextImp->activeTracerArray.load(std::memory_order_relaxed));
    return (void *)stableTracerArray;
}

void APITracerContextImp::releaseActivetracersList() {
    if (myThreadPrivateTracerData.testAndSetThreadTracerDataInitializedAndOnList())
        myThreadPrivateTracerData.tracerArrayPointer.store(nullptr, std::memory_order_relaxed);
}

} // namespace L0
