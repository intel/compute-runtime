/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "shared/source/helpers/debug_helpers.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <variant>

namespace DebugVariablesChangeTracker {
namespace {

struct Change {
    void *variable = nullptr;
    std::variant<bool, int32_t, int64_t, std::string> previousValue;
};

// Fixed capacity, because an allocation during a test is reported by MemoryLeakListener.
constexpr size_t maxChangesStackDepth = 1024;
std::array<Change, maxChangesStackDepth> changesStack;
size_t changesStackDepth = 0;
DebugVariables *trackedFlags = nullptr;

void revertChange(Change &change) {
    auto revertToPreviousValue = [&change](auto &previousValue) {
        using T = std::decay_t<decltype(previousValue)>;
        auto &value = const_cast<T &>(static_cast<DebugVarBase<T> *>(change.variable)->getRef());
        value = std::move(previousValue);
        if constexpr (std::is_same_v<T, std::string>) {
            value.shrink_to_fit();
        }
    };
    std::visit(revertToPreviousValue, change.previousValue);
}

bool isVariableOfTrackedFlags(const void *variable) {
    const auto flagsBegin = reinterpret_cast<const char *>(trackedFlags);
    const auto variableAddress = reinterpret_cast<const char *>(variable);
    return variableAddress >= flagsBegin && variableAddress < flagsBegin + sizeof(DebugVariables);
}

template <typename T>
void recordChange(DebugVarBase<T> &variable, const T &newValue) {
    const bool valueChanged = variable.getRef() != newValue;
    if (!isVariableOfTrackedFlags(&variable) || !valueChanged) {
        return;
    }
    if (changesStackDepth == changesStack.size()) {
        fprintf(stderr, "DebugVariablesChangeTracker: too many debug variable changes\n");
        abort();
    }
    auto &change = changesStack[changesStackDepth++];
    change.variable = &variable;
    change.previousValue = variable.get();
}

void revertChangesFrom(size_t firstChangeIndex) {
    while (changesStackDepth > firstChangeIndex) {
        auto &change = changesStack[--changesStackDepth];
        revertChange(change);
        change.previousValue = false;
    }
}

bool isVariableChangedBefore(size_t changeIndex) {
    for (size_t i = 0; i < changeIndex; i++) {
        if (changesStack[i].variable == changesStack[changeIndex].variable) {
            return true;
        }
    }
    return false;
}

size_t getNextChangeIndex() {
    UNRECOVERABLE_IF(trackedFlags == nullptr);
    return changesStackDepth;
}

} // namespace

void startRecording(DebugVariables &flags) {
    trackedFlags = &flags;
    DebugVarBase<bool>::changeHook = &recordChange<bool>;
    DebugVarBase<int32_t>::changeHook = &recordChange<int32_t>;
    DebugVarBase<int64_t>::changeHook = &recordChange<int64_t>;
    DebugVarBase<std::string>::changeHook = &recordChange<std::string>;
}

void stopRecording(void (*reportChangedVariable)(const void *variable)) {
    for (size_t i = 0; i < changesStackDepth; i++) {
        if (!isVariableChangedBefore(i)) {
            reportChangedVariable(changesStack[i].variable);
        }
    }
    revertChangesFrom(0);
    DebugVarBase<bool>::changeHook = nullptr;
    DebugVarBase<int32_t>::changeHook = nullptr;
    DebugVarBase<int64_t>::changeHook = nullptr;
    DebugVarBase<std::string>::changeHook = nullptr;
    trackedFlags = nullptr;
}

} // namespace DebugVariablesChangeTracker

DebugManagerStateRestore::DebugManagerStateRestore() : firstChangeIndex(DebugVariablesChangeTracker::getNextChangeIndex()), injectFcnSnapshot(debugManager.injectFcn) {}

DebugManagerStateRestore::~DebugManagerStateRestore() {
    debugManager.injectFcn = this->injectFcnSnapshot;
    DebugVariablesChangeTracker::revertChangesFrom(this->firstChangeIndex);
}
