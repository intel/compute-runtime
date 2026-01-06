/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>

template <typename T>
class VariableBackup {
  public:
    VariableBackup(T *ptr) : pValue(ptr) {
        oldValue = *ptr;
    }
    VariableBackup(T *ptr, T &&newValue) : pValue(ptr) {
        oldValue = std::move(*ptr);
        *pValue = std::move(newValue);
    }
    VariableBackup(T *ptr, T &newValue) : pValue(ptr) {
        oldValue = *ptr;
        *pValue = newValue;
    }
    ~VariableBackup() {
        *pValue = std::move(oldValue);
    }
    void operator=(const T &val) {
        *pValue = val;
    }
    template <typename T2>
    bool operator==(const T2 &val) const {
        return *pValue == val;
    }

  private:
    T oldValue;
    T *pValue;
};

template <typename T>
class NonCopyableVariableBackup {
  public:
    NonCopyableVariableBackup(T *ptr, T &&newValue) : pValue(ptr) {
        oldValue = std::move(*ptr);
        *pValue = std::move(newValue);
    }

    ~NonCopyableVariableBackup() {
        *pValue = std::move(oldValue);
    }

  private:
    T oldValue;
    T *pValue;
};