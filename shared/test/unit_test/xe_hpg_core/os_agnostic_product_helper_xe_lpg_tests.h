/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
class ProductHelper;
struct XeLpgTests {
    static void testOverridePatIndex(const ProductHelper &productHelper);
    static void testPreferredAllocationMethod(const ProductHelper &productHelper);
};
} // namespace NEO