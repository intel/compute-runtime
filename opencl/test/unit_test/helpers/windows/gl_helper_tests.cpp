/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/gl_helper.h"
#include "opencl/test/unit_test/helpers/windows/mock_function.h"

#include "gtest/gtest.h"

#include <memory>

typedef const char *(__cdecl *funcType)();

namespace NEO {
class GlFunctionHelperMock : public GlFunctionHelper {
  public:
    GlFunctionHelperMock(OsLibrary *glLibrary, const std::string &functionName) : GlFunctionHelper(glLibrary, functionName) {}
    using GlFunctionHelper::glFunctionPtr;
};

TEST(GlFunctionHelper, whenCreateGlFunctionHelperThenSetGlFunctionPtrToLoadAnotherFunctions) {
    std::unique_ptr<OsLibrary> glLibrary(OsLibrary::loadFunc({"mock_opengl32.dll"}));
    EXPECT_TRUE(glLibrary->isLoaded());
    GlFunctionHelperMock loader(glLibrary.get(), "mockLoader");
    funcType function1 = ConvertibleProcAddr{reinterpret_cast<void *>(loader.glFunctionPtr("realFunction"))};
    funcType function2 = loader["realFunction"];
    EXPECT_STREQ(function1(), function2());
}

TEST(GlFunctionHelper, givenNonExistingFunctionNameWhenCreateGlFunctionHelperThenNullptr) {
    std::unique_ptr<OsLibrary> glLibrary(OsLibrary::loadFunc({"mock_opengl32.dll"}));
    EXPECT_TRUE(glLibrary->isLoaded());
    GlFunctionHelper loader(glLibrary.get(), "mockLoader");
    funcType function = loader["nonExistingFunction"];
    EXPECT_EQ(nullptr, function);
}

TEST(GlFunctionHelper, givenRealFunctionNameWhenCreateGlFunctionHelperThenGetPointerToAppropriateFunction) {
    std::unique_ptr<OsLibrary> glLibrary(OsLibrary::loadFunc({"mock_opengl32.dll"}));
    EXPECT_TRUE(glLibrary->isLoaded());
    GlFunctionHelper loader(glLibrary.get(), "mockLoader");
    funcType function = loader["realFunction"];
    EXPECT_STREQ(realFunction(), function());
}
}; // namespace NEO
