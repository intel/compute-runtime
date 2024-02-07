/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/options.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/gdi_interface_logging.h"
#include "shared/source/os_interface/windows/gdi_profiling.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/api_intercept.h"

#include <chrono>
#include <iostream>
#include <string>

namespace NEO {

template <typename Param>
class ThkWrapper {
    typedef NTSTATUS(APIENTRY *Func)(Param);
    GdiProfiler &profiler;
    const std::string name{};
    const uint32_t id{};

  public:
    ThkWrapper(GdiProfiler &profiler, const char *name, uint32_t id) : profiler(profiler), name(name), id(id){};

    Func mFunc = nullptr;

    inline NTSTATUS operator()(Param param) const {
        if constexpr (GdiLogging::gdiLoggingSupport) {
            GdiLogging::logEnter<Param>(param);

            auto measureTime = debugManager.flags.PrintKmdTimes.get();
            std::chrono::steady_clock::time_point start;
            std::chrono::steady_clock::time_point end;

            if (measureTime) {
                start = std::chrono::steady_clock::now();
            }

            auto ret = mFunc(param);

            if (measureTime) {
                end = std::chrono::steady_clock::now();
                long long elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                profiler.recordElapsedTime(elapsedTime, this->name.c_str(), this->id);
            }

            GdiLogging::logExit<Param>(ret, param);

            return ret;
        } else {
            return mFunc(param);
        }
    }

    ThkWrapper &operator=(void *func) {
        mFunc = reinterpret_cast<decltype(mFunc)>(func);
        return *this;
    }

    ThkWrapper &operator=(Func func) {
        mFunc = func;
        return *this;
    }

    // This operator overload is for implicit casting ThkWrapper struct to Function Pointer in GetPfn methods like GetEscapePfn() or for comparing against NULL function pointer
    operator Func() const {
        return mFunc;
    }
};

} // namespace NEO
