/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

template <typename T>
class VariableBackup {
  public:
    VariableBackup(T *ptr) : pValue(ptr) {
        oldValue = *ptr;
    }
    VariableBackup(T *ptr, T &&newValue) : pValue(ptr) {
        oldValue = *ptr;
        *pValue = newValue;
    }
    VariableBackup(T *ptr, T &newValue) : pValue(ptr) {
        oldValue = *ptr;
        *pValue = newValue;
    }
    ~VariableBackup() {
        *pValue = oldValue;
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
