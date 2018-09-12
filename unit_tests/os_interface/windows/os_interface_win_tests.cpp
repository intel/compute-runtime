/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/os_interface/windows/os_context_win.h"
#include "unit_tests/os_interface/windows/os_interface_win_tests.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

TEST_F(OsInterfaceTest, givenOsInterfaceWithoutWddmWhenGetHwContextIdIsCalledThenReturnsZero) {
    auto retVal = osInterface->getHwContextId();
    EXPECT_EQ(0u, retVal);
}

TEST_F(OsInterfaceTest, GivenWindowsWhenOsSupportFor64KBpagesIsBeingQueriedThenTrueIsReturned) {
    EXPECT_TRUE(OSInterface::are64kbPagesEnabled());
}

TEST_F(OsInterfaceTest, GivenWindowsWhenCreateEentIsCalledThenValidEventHandleIsReturned) {
    auto ev = osInterface->get()->createEvent(NULL, TRUE, FALSE, "DUMMY_EVENT_NAME");
    EXPECT_NE(nullptr, ev);
    auto ret = osInterface->get()->closeHandle(ev);
    EXPECT_EQ(TRUE, ret);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextBeforeInitWddmThenOsContextIsNotInitialized) {
    auto wddm = new WddmMock;
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    EXPECT_THROW(auto osContext = std::make_unique<OsContext>(&osInterface, 0u), std::exception);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextAfterInitWddmThenOsContextIsInitialized) {
    auto wddm = new WddmMock;
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    wddm->init();
    auto osContext = std::make_unique<OsContext>(&osInterface, 0u);
    EXPECT_NE(nullptr, osContext->get());
    EXPECT_TRUE(osContext->get()->isInitialized());
    EXPECT_EQ(osContext->get()->getWddm(), wddm);
}

TEST(OsContextTest, whenCreateOsContextWithoutOsInterfaceThenOsContextImplIsNotAvailable) {
    auto osContext = std::make_unique<OsContext>(nullptr, 0u);
    EXPECT_EQ(nullptr, osContext->get());
}
