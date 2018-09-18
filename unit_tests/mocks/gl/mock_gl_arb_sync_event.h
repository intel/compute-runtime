/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/sharings/gl/gl_arb_sync_event.h"

template <bool FailCreation>
struct DummyArbEvent : OCLRT::GlArbSyncEvent {
    DummyArbEvent(OCLRT::Context &ctx)
        : GlArbSyncEvent(ctx) {
    }

    using GlArbSyncEvent::baseEvent;
    using GlArbSyncEvent::glSyncInfo;
    using GlArbSyncEvent::osInterface;
    bool useBaseSetEvent = false;

    bool setBaseEvent(Event &ev) override {
        return GlArbSyncEvent::setBaseEvent(ev);
    }

    ~DummyArbEvent() override {
        GlArbSyncEvent::glSyncInfo.reset();
    }

    static GlArbSyncEvent *create(Event &baseEv) {
        if (FailCreation) {
            return nullptr;
        }
        auto syncEv = new DummyArbEvent<FailCreation>(*baseEv.getContext());
        syncEv->baseEvent = &baseEv;
        return syncEv;
    }
};

inline void glArbSyncObjectCleanupMockDoNothing(OCLRT::OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo) {
}

inline void glArbSyncObjectSignalMockDoNothing(OCLRT::OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {
}

template <bool Fail>
inline bool mockGlArbSyncObjectSetup(OCLRT::GLSharingFunctions &sharing, OCLRT::OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    return (Fail == false);
}
