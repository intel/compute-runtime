/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

thread_local ze_bool_t tracingInProgress = 0;
bool tracingIsEnabled = false;

struct APITracerContextImp GLOBAL_APITracerContextImp;
struct APITracerContextImp *PGLOBAL_APITracerContextImp = &GLOBAL_APITracerContextImp;

APITracer *APITracer::create() {
    APITracerImp *tracer = new APITracerImp;
    tracer->tracingState = disabledState;
    tracer->tracerFunctions = {};
    UNRECOVERABLE_IF(tracer == nullptr);
    return tracer;
}

ze_result_t createAPITracer(zet_driver_handle_t hDriver, const zet_tracer_desc_t *desc, zet_tracer_handle_t *phTracer) {

    if (!PGLOBAL_APITracerContextImp->isTracingEnabled()) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    APITracerImp *tracer = static_cast<APITracerImp *>(APITracer::create());

    tracer->tracerFunctions.pUserData = desc->pUserData;

    *phTracer = tracer->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t APITracerImp::destroyTracer(zet_tracer_handle_t phTracer) {

    APITracerImp *tracer = static_cast<APITracerImp *>(phTracer);

    ze_result_t result = PGLOBAL_APITracerContextImp->finalizeDisableImpTracingWait(tracer);
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
    return PGLOBAL_APITracerContextImp->enableTracingImp(this, enable);
}

static std::mutex perThreadTracerDataMutex;

static std::list<per_thread_tracer_data_t *> perThreadTracerDataList;

void ThreadPrivateTracerData::allocatePerThreadPublicTracerData() {
    if (myThreadPublicTracerData == nullptr) {
        myThreadPublicTracerData = new per_thread_tracer_data_t;
        myThreadPublicTracerData->tracerArrayPointer.store(NULL, std::memory_order_relaxed);
        myThreadPublicTracerData->thread_id = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(perThreadTracerDataMutex);
        perThreadTracerDataList.push_back(myThreadPublicTracerData);
    }
}

void ThreadPrivateTracerData::freePerThreadPublicTracerData() {
    //
    // There is no need to hold a mutex when testing
    // my_thread_tracer_data is a thread_local object.
    // my_threadd_tracer_data for nullptr since it can only be done
    // within the current thread's context.
    // So there can be no other racing threads.
    //
    if (myThreadPublicTracerData != nullptr) {
        std::lock_guard<std::mutex> lock(perThreadTracerDataMutex);
        perThreadTracerDataList.remove(myThreadPublicTracerData);
        delete myThreadPublicTracerData;
        myThreadPublicTracerData = nullptr;
    }
}

ThreadPrivateTracerData::ThreadPrivateTracerData() {
    myThreadPublicTracerData = nullptr;
}

ThreadPrivateTracerData::~ThreadPrivateTracerData() {
    freePerThreadPublicTracerData();
}

thread_local ThreadPrivateTracerData myThreadPrivateTracerData;

//
// This thread_local allows for an optimisation of the test_for_tracer_array_references()
// function.  The optimization adds a test and branch, but it allows the common code path
// to avoid TWO out of line function calls.
//
// One function call is to call the constructor for the thread_private_tracer_data class.
// Note that this call is probably pretty heavy-weight, because it needs to be thread safe.
// It MUST include a mutex.
//
// The second function call we avoid is the call to the thread_private_tracer_data class's
// allocate memory member. It appears that at least with the Linux g++ compiler,
// the "inline" annotation on a member function is accepted at compile time, but does not
// change the code that is generated.
//
static thread_local bool myThreadPrivateTracerDataIsInitialized = false;

void APITracerContextImp::apiTracingEnable(ze_init_flag_t flag) {
    if (driver_ddiTable.enableTracing) {
        tracingIsEnabled = true;
    }
}

void APITracerContextImp::enableTracing() { tracingIsEnabled = true; }
void APITracerContextImp::disableTracing() { tracingIsEnabled = false; }
bool APITracerContextImp::isTracingEnabled() { return tracingIsEnabled; }

//
// Walk the list of per-thread private data structures, testing
// whether any of them reference this array.
//
// Return 1 if a reference is found.  Otherwise return 0.
//
ze_bool_t APITracerContextImp::testForTracerArrayReferences(tracer_array_t *tracerArray) {
    std::list<per_thread_tracer_data_t *>::iterator itr;
    for (itr = perThreadTracerDataList.begin();
         itr != perThreadTracerDataList.end();
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

int APITracerContextImp::updateTracerArrays() {
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
    tracer_array_t *active_tracer_array_shadow = activeTracerArray.load(std::memory_order_relaxed);
    if (active_tracer_array_shadow != &emptyTracerArray) {
        retiringTracerArrayList.push_back(active_tracer_array_shadow);
    }
    //
    // This active_tracer_array.store must use memory_order_release.
    // This store DOES signal a logical transfer of tracer state information
    // from this thread to the tracing threads.
    //
    activeTracerArray.store(newTracerArray, std::memory_order_release);
    testAndFreeRetiredTracers();

    return 0;
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
            updateTracerArrays();
        }
        result = ZE_RESULT_SUCCESS;
        break;

    case disabledWaitingState:
        result = ZE_RESULT_ERROR_UNINITIALIZED;
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
        result = ZE_RESULT_ERROR_UNINITIALIZED;
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

//
// For an explanation of this function and the reason for its while loop,
// see the comments at the top of this file.
//
void *APITracerContextImp::getActiveTracersList() {
    tracer_array_t *stableTracerArray = NULL;
    //
    // This test and branch allows us to avoid TWO function calls.  One call is for the
    // constructor for my_thread_private_tracer_data.  The other is to avoid the function
    // call to allocate_per_thread_tracer_data().
    //
    // Since my_thread_private_tracer_data_is_initialized and my_thread_private_tracer_data are
    // thread_local, there is no thread safety issue here. Each thread will find
    // my_thread_private_tracer_data_is_initialized to be "false" at most once.
    //
    if (!myThreadPrivateTracerDataIsInitialized) {
        myThreadPrivateTracerData.allocatePerThreadPublicTracerData();
        myThreadPrivateTracerDataIsInitialized = true;
    }
    do {
        //
        // This read of active_tracer_array DOES logically signal a transfer
        // of tracer structure information from the threader enable/disable/destroy
        // thread to this tracing thread.  So it must use memory_order_acquire
        //
        stableTracerArray = PGLOBAL_APITracerContextImp->activeTracerArray.load(std::memory_order_acquire);
        myThreadPrivateTracerData.myThreadPublicTracerData->tracerArrayPointer.store(stableTracerArray, std::memory_order_relaxed);
        //
        // This read of active_tracer_array does NOT transfer any information
        // that was not already transferred by the previous read within this loop.
        // So it can use memory_order_relaxed.
        //
    } while (stableTracerArray !=
             PGLOBAL_APITracerContextImp->activeTracerArray.load(std::memory_order_relaxed));
    return (void *)stableTracerArray;
}

void APITracerContextImp::releaseActivetracersList() {
    myThreadPrivateTracerData.myThreadPublicTracerData->tracerArrayPointer.store(NULL, std::memory_order_relaxed);
}

} // namespace L0
