/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/ddi/ze_ddi_tables.h"
#include "level_zero/experimental/source/tracing/tracing.h"
#include "level_zero/experimental/source/tracing/tracing_barrier_imp.h"
#include "level_zero/experimental/source/tracing/tracing_cmdlist_imp.h"
#include "level_zero/experimental/source/tracing/tracing_cmdqueue_imp.h"
#include "level_zero/experimental/source/tracing/tracing_copy_imp.h"
#include "level_zero/experimental/source/tracing/tracing_device_imp.h"
#include "level_zero/experimental/source/tracing/tracing_driver_imp.h"
#include "level_zero/experimental/source/tracing/tracing_event_imp.h"
#include "level_zero/experimental/source/tracing/tracing_fence_imp.h"
#include "level_zero/experimental/source/tracing/tracing_global_imp.h"
#include "level_zero/experimental/source/tracing/tracing_image_imp.h"
#include "level_zero/experimental/source/tracing/tracing_memory_imp.h"
#include "level_zero/experimental/source/tracing/tracing_module_imp.h"
#include "level_zero/experimental/source/tracing/tracing_residency_imp.h"
#include "level_zero/experimental/source/tracing/tracing_sampler_imp.h"
#include <level_zero/ze_api.h>

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <vector>

extern ze_gpu_driver_dditable_t driverDdiTable;

namespace L0 {

extern thread_local ze_bool_t tracingInProgress;
extern struct APITracerContextImp *pGlobalAPITracerContextImp;

typedef struct TracerArrayEntry {
    zet_core_callbacks_t corePrologues;
    zet_core_callbacks_t coreEpilogues;
    zet_device_handle_t hDevice;
    void *pUserData;
} tracer_array_entry_t;

typedef struct TracerArray {
    size_t tracerArrayCount;
    tracer_array_entry_t *tracerArrayEntries;
} tracer_array_t;

enum TracingState {
    disabledState,        // tracing has never been enabled
    enabledState,         // tracing is enabled.
    disabledWaitingState, // tracing has been disabled, but not waited for
};

struct APITracerImp : APITracer {
    ze_result_t destroyTracer(zet_tracer_exp_handle_t phTracer) override;
    ze_result_t setPrologues(zet_core_callbacks_t *pCoreCbs) override;
    ze_result_t setEpilogues(zet_core_callbacks_t *pCoreCbs) override;
    ze_result_t enableTracer(ze_bool_t enable) override;

    tracer_array_entry_t tracerFunctions{};
    TracingState tracingState = disabledState;

  private:
};

class ThreadPrivateTracerData {
  public:
    void clearThreadTracerDataOnList(void) { onList = false; }
    void removeThreadTracerDataFromList(void);
    bool testAndSetThreadTracerDataInitializedAndOnList(void);
    bool onList;
    bool isInitialized;
    ThreadPrivateTracerData();
    ~ThreadPrivateTracerData();

    std::atomic<tracer_array_t *> tracerArrayPointer;

  private:
    ThreadPrivateTracerData(const ThreadPrivateTracerData &);
    ThreadPrivateTracerData &operator=(const ThreadPrivateTracerData &);
};

struct APITracerContextImp {
  public:
    APITracerContextImp() {
        activeTracerArray.store(&emptyTracerArray, std::memory_order_relaxed);
    };

    ~APITracerContextImp();

    static void apiTracingEnable(ze_init_flag_t flag);

    void *getActiveTracersList();
    void releaseActivetracersList();

    ze_result_t enableTracingImp(struct APITracerImp *newTracer, ze_bool_t enable);
    ze_result_t finalizeDisableImpTracingWait(struct APITracerImp *oldTracer);

    bool isTracingEnabled();

    void addThreadTracerDataToList(ThreadPrivateTracerData *threadDataP);
    void removeThreadTracerDataFromList(ThreadPrivateTracerData *threadDataP);

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
    size_t updateTracerArrays();

    std::list<ThreadPrivateTracerData *> threadTracerDataList;
    std::mutex threadTracerDataListMutex;
};

extern thread_local ThreadPrivateTracerData myThreadPrivateTracerData;

template <class T>
class APITracerCallbackStateImp {
  public:
    T currentApiCallback;
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

#define ZE_GEN_PER_API_CALLBACK_STATE(perApiCallbackData, tracerType, callbackCategory, callbackFunctionType)                               \
    L0::tracer_array_t *currentTracerArray;                                                                                                 \
    currentTracerArray = (L0::tracer_array_t *)L0::pGlobalAPITracerContextImp->getActiveTracersList();                                      \
    if (currentTracerArray) {                                                                                                               \
        for (size_t i = 0; i < currentTracerArray->tracerArrayCount; i++) {                                                                 \
            tracerType prologueCallbackPtr;                                                                                                 \
            tracerType epilogue_callback_ptr;                                                                                               \
            ZE_GEN_TRACER_ARRAY_ENTRY(prologueCallbackPtr, currentTracerArray, i, corePrologues, callbackCategory, callbackFunctionType);   \
            ZE_GEN_TRACER_ARRAY_ENTRY(epilogue_callback_ptr, currentTracerArray, i, coreEpilogues, callbackCategory, callbackFunctionType); \
                                                                                                                                            \
            L0::APITracerCallbackStateImp<tracerType> prologCallback;                                                                       \
            prologCallback.currentApiCallback = prologueCallbackPtr;                                                                        \
            prologCallback.pUserData = currentTracerArray->tracerArrayEntries[i].pUserData;                                                 \
            perApiCallbackData.prologCallbacks.push_back(prologCallback);                                                                   \
                                                                                                                                            \
            L0::APITracerCallbackStateImp<tracerType> epilogCallback;                                                                       \
            epilogCallback.currentApiCallback = epilogue_callback_ptr;                                                                      \
            epilogCallback.pUserData = currentTracerArray->tracerArrayEntries[i].pUserData;                                                 \
            perApiCallbackData.epilogCallbacks.push_back(epilogCallback);                                                                   \
        }                                                                                                                                   \
    }

template <typename TFunctionPointer, typename TParams, typename TTracer, typename TTracerPrologCallbacks, typename TTracerEpilogCallbacks, typename... Args>
ze_result_t apiTracerWrapperImp(TFunctionPointer zeApiPtr,
                                TParams paramsStruct,
                                TTracer apiOrdinal,
                                TTracerPrologCallbacks prologCallbacks,
                                TTracerEpilogCallbacks epilogCallbacks,
                                Args &&...args) {
    ze_result_t ret = ZE_RESULT_SUCCESS;
    std::vector<APITracerCallbackStateImp<TTracer>> *callbacksPrologs = &prologCallbacks;

    std::vector<void *> ppTracerInstanceUserData;
    ppTracerInstanceUserData.resize(callbacksPrologs->size());

    for (size_t i = 0; i < callbacksPrologs->size(); i++) {
        if (callbacksPrologs->at(i).currentApiCallback != nullptr)
            callbacksPrologs->at(i).currentApiCallback(paramsStruct, ret, callbacksPrologs->at(i).pUserData, &ppTracerInstanceUserData[i]);
    }
    ret = zeApiPtr(args...);
    std::vector<APITracerCallbackStateImp<TTracer>> *callbacksEpilogs = &epilogCallbacks;
    for (size_t i = 0; i < callbacksEpilogs->size(); i++) {
        if (callbacksEpilogs->at(i).currentApiCallback != nullptr)
            callbacksEpilogs->at(i).currentApiCallback(paramsStruct, ret, callbacksEpilogs->at(i).pUserData, &ppTracerInstanceUserData[i]);
    }
    L0::tracingInProgress = 0;
    L0::pGlobalAPITracerContextImp->releaseActivetracersList();
    return ret;
}

} // namespace L0
