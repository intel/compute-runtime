/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/tracing/tracing.h"
#include "level_zero/tools/source/tracing/tracing_barrier_imp.h"
#include "level_zero/tools/source/tracing/tracing_cl_interop_imp.h"
#include "level_zero/tools/source/tracing/tracing_cmdlist_imp.h"
#include "level_zero/tools/source/tracing/tracing_cmdqueue_imp.h"
#include "level_zero/tools/source/tracing/tracing_copy_imp.h"
#include "level_zero/tools/source/tracing/tracing_device_imp.h"
#include "level_zero/tools/source/tracing/tracing_driver_imp.h"
#include "level_zero/tools/source/tracing/tracing_event_imp.h"
#include "level_zero/tools/source/tracing/tracing_fence_imp.h"
#include "level_zero/tools/source/tracing/tracing_global_imp.h"
#include "level_zero/tools/source/tracing/tracing_image_imp.h"
#include "level_zero/tools/source/tracing/tracing_memory_imp.h"
#include "level_zero/tools/source/tracing/tracing_module_imp.h"
#include "level_zero/tools/source/tracing/tracing_residency_imp.h"
#include "level_zero/tools/source/tracing/tracing_sampler_imp.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

#include "ze_ddi_tables.h"

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

extern ze_gpu_driver_dditable_t driver_ddiTable;

namespace L0 {

extern thread_local ze_bool_t tracingInProgress;
extern struct APITracerContextImp *PGLOBAL_APITracerContextImp;

typedef struct tracer_array_entry {
    zet_core_callbacks_t corePrologues;
    zet_core_callbacks_t coreEpilogues;
    zet_device_handle_t hDevice;
    void *pUserData;
} tracer_array_entry_t;

typedef struct tracerArray {
    size_t tracerArrayCount;
    tracer_array_entry_t *tracerArrayEntries;
} tracer_array_t;

typedef struct per_thread_public_tracer_data {
    std::atomic<tracer_array_t *> tracerArrayPointer;
    std::thread::id thread_id;
} per_thread_tracer_data_t;

class ThreadPrivateTracerData {
  public:
    per_thread_tracer_data_t *myThreadPublicTracerData;
    void allocatePerThreadPublicTracerData();
    void freePerThreadPublicTracerData();
    ThreadPrivateTracerData();
    ~ThreadPrivateTracerData();

  private:
    ThreadPrivateTracerData(const ThreadPrivateTracerData &);
    ThreadPrivateTracerData &operator=(const ThreadPrivateTracerData &);
};

extern thread_local ThreadPrivateTracerData myThreadPrivateTracerData;

typedef enum tracingState {
    disabledState,        // tracing has never been enabled
    enabledState,         // tracing is enabled.
    disabledWaitingState, // tracing has been disabled, but not waited for
} tracingState_t;

struct APITracerImp : APITracer {
    ze_result_t destroyTracer(zet_tracer_handle_t phTracer) override;
    ze_result_t setPrologues(zet_core_callbacks_t *pCoreCbs) override;
    ze_result_t setEpilogues(zet_core_callbacks_t *pCoreCbs) override;
    ze_result_t enableTracer(ze_bool_t enable) override;

    tracer_array_entry_t tracerFunctions;
    tracingState_t tracingState;

  private:
};

struct APITracerContextImp : APITracerContext {
  public:
    APITracerContextImp() {
        activeTracerArray.store(&emptyTracerArray, std::memory_order_relaxed);
    };

    ~APITracerContextImp() override {}

    static void apiTracingEnable(ze_init_flag_t flag);

    void *getActiveTracersList() override;
    void releaseActivetracersList() override;

    ze_result_t enableTracingImp(struct APITracerImp *newTracer, ze_bool_t enable);
    ze_result_t finalizeDisableImpTracingWait(struct APITracerImp *oldTracer);

    void enableTracing();
    void disableTracing();
    bool isTracingEnabled();

  private:
    std::mutex traceTableMutex;
    tracer_array_t emptyTracerArray = {0, NULL};
    std::atomic<tracer_array_t *> activeTracerArray;

    //
    // a list of tracer arrays that were once active, but
    // have been replaced by a new active array.  These
    // once-active tracer arrays may continue for some time
    // to have references to them among the per-thread
    // tracer array pointers.
    //
    std::list<tracer_array_t *> retiringTracerArrayList;

    std::list<struct APITracerImp *> enabledTracerImpList;

    ze_bool_t testForTracerArrayReferences(tracer_array_t *tracerArray);
    size_t testAndFreeRetiredTracers();
    int updateTracerArrays();
};

template <class T>
class APITracerCallbackStateImp {
  public:
    T current_api_callback;
    void *pUserData;
};

template <class T>
class APITracerCallbackDataImp {
  public:
    T apiOrdinal = {};
    std::vector<L0::APITracerCallbackStateImp<T>> prologCallbacks;
    std::vector<L0::APITracerCallbackStateImp<T>> epilogCallbacks;
};

#define ZE_HANDLE_TRACER_RECURSION(ze_api_ptr, ...) \
    do {                                            \
        if (L0::tracingInProgress) {                \
            return ze_api_ptr(__VA_ARGS__);         \
        }                                           \
        L0::tracingInProgress = 1;                  \
    } while (0)

#define ZE_GEN_TRACER_ARRAY_ENTRY(callbackPtr, tracerArray, tracerArrayIndex, callbackType, callbackCategory, callbackFunction) \
    do {                                                                                                                        \
        callbackPtr = tracerArray->tracerArrayEntries[tracerArrayIndex].callbackType.callbackCategory.callbackFunction;         \
    } while (0)

#define ZE_GEN_PER_API_CALLBACK_STATE(perApiCallbackData, tracerType, callbackCategory, callbackFunctionType)                           \
    L0::tracer_array_t *currentTracerArray;                                                                                             \
    currentTracerArray = (L0::tracer_array_t *)L0::PGLOBAL_APITracerContextImp->getActiveTracersList();                                 \
    for (size_t i = 0; i < currentTracerArray->tracerArrayCount; i++) {                                                                 \
        tracerType prologueCallbackPtr;                                                                                                 \
        tracerType epilogue_callback_ptr;                                                                                               \
        ZE_GEN_TRACER_ARRAY_ENTRY(prologueCallbackPtr, currentTracerArray, i, corePrologues, callbackCategory, callbackFunctionType);   \
        ZE_GEN_TRACER_ARRAY_ENTRY(epilogue_callback_ptr, currentTracerArray, i, coreEpilogues, callbackCategory, callbackFunctionType); \
                                                                                                                                        \
        L0::APITracerCallbackStateImp<tracerType> prologCallback;                                                                       \
        prologCallback.current_api_callback = prologueCallbackPtr;                                                                      \
        prologCallback.pUserData = currentTracerArray->tracerArrayEntries[i].pUserData;                                                 \
        perApiCallbackData.prologCallbacks.push_back(prologCallback);                                                                   \
                                                                                                                                        \
        L0::APITracerCallbackStateImp<tracerType> epilogCallback;                                                                       \
        epilogCallback.current_api_callback = epilogue_callback_ptr;                                                                    \
        epilogCallback.pUserData = currentTracerArray->tracerArrayEntries[i].pUserData;                                                 \
        perApiCallbackData.epilogCallbacks.push_back(epilogCallback);                                                                   \
    }

template <typename TFunction_pointer, typename TParams, typename TTracer, typename TTracerPrologCallbacks, typename TTracerEpilogCallbacks, typename... Args>
ze_result_t APITracerWrapperImp(TFunction_pointer zeApiPtr,
                                TParams paramsStruct,
                                TTracer apiOrdinal,
                                TTracerPrologCallbacks prologCallbacks,
                                TTracerEpilogCallbacks epilogCallbacks,
                                Args &&... args) {
    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::vector<APITracerCallbackStateImp<TTracer>> *callbacks_prologs = &prologCallbacks;

    std::vector<void *> ppTracerInstanceUserData;
    ppTracerInstanceUserData.resize(callbacks_prologs->size());

    for (size_t i = 0; i < callbacks_prologs->size(); i++) {
        if (callbacks_prologs->at(i).current_api_callback != nullptr)
            callbacks_prologs->at(i).current_api_callback(paramsStruct, ret, callbacks_prologs->at(i).pUserData, &ppTracerInstanceUserData[i]);
    }
    ret = zeApiPtr(args...);
    std::vector<APITracerCallbackStateImp<TTracer>> *callbacksEpilogs = &epilogCallbacks;
    for (size_t i = 0; i < callbacksEpilogs->size(); i++) {
        if (callbacksEpilogs->at(i).current_api_callback != nullptr)
            callbacksEpilogs->at(i).current_api_callback(paramsStruct, ret, callbacksEpilogs->at(i).pUserData, &ppTracerInstanceUserData[i]);
    }
    L0::tracingInProgress = 0;
    L0::PGLOBAL_APITracerContextImp->releaseActivetracersList();
    return ret;
}

} // namespace L0
