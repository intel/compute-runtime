/*
 * Copyright (C) 2017-2020 Intel Corporation
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

  private:
    T oldValue;
    T *pValue;
};
