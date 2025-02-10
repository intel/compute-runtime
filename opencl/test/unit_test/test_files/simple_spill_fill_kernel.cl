/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define SIZE 128
#define NO_TAIL()

#define LEXP_0(eval_macro, tail_macro) tail_macro()
#define LEXP_1(eval_macro, tail_macro)                                         \
  eval_macro(0) LEXP_0(eval_macro, tail_macro)
#define LEXP_2(eval_macro, tail_macro)                                         \
  eval_macro(1) LEXP_1(eval_macro, tail_macro)
#define LEXP_3(eval_macro, tail_macro)                                         \
  eval_macro(2) LEXP_2(eval_macro, tail_macro)
#define LEXP_4(eval_macro, tail_macro)                                         \
  eval_macro(3) LEXP_3(eval_macro, tail_macro)
#define LEXP_5(eval_macro, tail_macro)                                         \
  eval_macro(4) LEXP_4(eval_macro, tail_macro)
#define LEXP_6(eval_macro, tail_macro)                                         \
  eval_macro(5) LEXP_5(eval_macro, tail_macro)
#define LEXP_7(eval_macro, tail_macro)                                         \
  eval_macro(6) LEXP_6(eval_macro, tail_macro)
#define LEXP_8(eval_macro, tail_macro)                                         \
  eval_macro(7) LEXP_7(eval_macro, tail_macro)
#define LEXP_9(eval_macro, tail_macro)                                         \
  eval_macro(8) LEXP_8(eval_macro, tail_macro)
#define LEXP_10(eval_macro, tail_macro)                                        \
  eval_macro(9) LEXP_9(eval_macro, tail_macro)
#define LEXP_11(eval_macro, tail_macro)                                        \
  eval_macro(10) LEXP_10(eval_macro, tail_macro)
#define LEXP_12(eval_macro, tail_macro)                                        \
  eval_macro(11) LEXP_11(eval_macro, tail_macro)
#define LEXP_13(eval_macro, tail_macro)                                        \
  eval_macro(12) LEXP_12(eval_macro, tail_macro)
#define LEXP_14(eval_macro, tail_macro)                                        \
  eval_macro(13) LEXP_13(eval_macro, tail_macro)
#define LEXP_15(eval_macro, tail_macro)                                        \
  eval_macro(14) LEXP_14(eval_macro, tail_macro)
#define LEXP_16(eval_macro, tail_macro)                                        \
  eval_macro(15) LEXP_15(eval_macro, tail_macro)
#define LEXP_17(eval_macro, tail_macro)                                        \
  eval_macro(16) LEXP_16(eval_macro, tail_macro)
#define LEXP_18(eval_macro, tail_macro)                                        \
  eval_macro(17) LEXP_17(eval_macro, tail_macro)
#define LEXP_19(eval_macro, tail_macro)                                        \
  eval_macro(18) LEXP_18(eval_macro, tail_macro)
#define LEXP_20(eval_macro, tail_macro)                                        \
  eval_macro(19) LEXP_19(eval_macro, tail_macro)
#define LEXP_21(eval_macro, tail_macro)                                        \
  eval_macro(20) LEXP_20(eval_macro, tail_macro)
#define LEXP_22(eval_macro, tail_macro)                                        \
  eval_macro(21) LEXP_21(eval_macro, tail_macro)
#define LEXP_23(eval_macro, tail_macro)                                        \
  eval_macro(22) LEXP_22(eval_macro, tail_macro)
#define LEXP_24(eval_macro, tail_macro)                                        \
  eval_macro(23) LEXP_23(eval_macro, tail_macro)
#define LEXP_25(eval_macro, tail_macro)                                        \
  eval_macro(24) LEXP_24(eval_macro, tail_macro)
#define LEXP_26(eval_macro, tail_macro)                                        \
  eval_macro(25) LEXP_25(eval_macro, tail_macro)
#define LEXP_27(eval_macro, tail_macro)                                        \
  eval_macro(26) LEXP_26(eval_macro, tail_macro)
#define LEXP_28(eval_macro, tail_macro)                                        \
  eval_macro(27) LEXP_27(eval_macro, tail_macro)
#define LEXP_29(eval_macro, tail_macro)                                        \
  eval_macro(28) LEXP_28(eval_macro, tail_macro)
#define LEXP_30(eval_macro, tail_macro)                                        \
  eval_macro(29) LEXP_29(eval_macro, tail_macro)
#define LEXP_31(eval_macro, tail_macro)                                        \
  eval_macro(30) LEXP_30(eval_macro, tail_macro)
#define LEXP_32(eval_macro, tail_macro)                                        \
  eval_macro(31) LEXP_31(eval_macro, tail_macro)
#define LEXP_33(eval_macro, tail_macro)                                        \
  eval_macro(32) LEXP_32(eval_macro, tail_macro)
#define LEXP_34(eval_macro, tail_macro)                                        \
  eval_macro(33) LEXP_33(eval_macro, tail_macro)
#define LEXP_35(eval_macro, tail_macro)                                        \
  eval_macro(34) LEXP_34(eval_macro, tail_macro)
#define LEXP_36(eval_macro, tail_macro)                                        \
  eval_macro(35) LEXP_35(eval_macro, tail_macro)
#define LEXP_37(eval_macro, tail_macro)                                        \
  eval_macro(36) LEXP_36(eval_macro, tail_macro)
#define LEXP_38(eval_macro, tail_macro)                                        \
  eval_macro(37) LEXP_37(eval_macro, tail_macro)
#define LEXP_39(eval_macro, tail_macro)                                        \
  eval_macro(38) LEXP_38(eval_macro, tail_macro)
#define LEXP_40(eval_macro, tail_macro)                                        \
  eval_macro(39) LEXP_39(eval_macro, tail_macro)
#define LEXP_41(eval_macro, tail_macro)                                        \
  eval_macro(40) LEXP_40(eval_macro, tail_macro)
#define LEXP_42(eval_macro, tail_macro)                                        \
  eval_macro(41) LEXP_41(eval_macro, tail_macro)
#define LEXP_43(eval_macro, tail_macro)                                        \
  eval_macro(42) LEXP_42(eval_macro, tail_macro)
#define LEXP_44(eval_macro, tail_macro)                                        \
  eval_macro(43) LEXP_43(eval_macro, tail_macro)
#define LEXP_45(eval_macro, tail_macro)                                        \
  eval_macro(44) LEXP_44(eval_macro, tail_macro)
#define LEXP_46(eval_macro, tail_macro)                                        \
  eval_macro(45) LEXP_45(eval_macro, tail_macro)
#define LEXP_47(eval_macro, tail_macro)                                        \
  eval_macro(46) LEXP_46(eval_macro, tail_macro)
#define LEXP_48(eval_macro, tail_macro)                                        \
  eval_macro(47) LEXP_47(eval_macro, tail_macro)
#define LEXP_49(eval_macro, tail_macro)                                        \
  eval_macro(48) LEXP_48(eval_macro, tail_macro)
#define LEXP_50(eval_macro, tail_macro)                                        \
  eval_macro(49) LEXP_49(eval_macro, tail_macro)
#define LEXP_51(eval_macro, tail_macro)                                        \
  eval_macro(50) LEXP_50(eval_macro, tail_macro)
#define LEXP_52(eval_macro, tail_macro)                                        \
  eval_macro(51) LEXP_51(eval_macro, tail_macro)
#define LEXP_53(eval_macro, tail_macro)                                        \
  eval_macro(52) LEXP_52(eval_macro, tail_macro)
#define LEXP_54(eval_macro, tail_macro)                                        \
  eval_macro(53) LEXP_53(eval_macro, tail_macro)
#define LEXP_55(eval_macro, tail_macro)                                        \
  eval_macro(54) LEXP_54(eval_macro, tail_macro)
#define LEXP_56(eval_macro, tail_macro)                                        \
  eval_macro(55) LEXP_55(eval_macro, tail_macro)
#define LEXP_57(eval_macro, tail_macro)                                        \
  eval_macro(56) LEXP_56(eval_macro, tail_macro)
#define LEXP_58(eval_macro, tail_macro)                                        \
  eval_macro(57) LEXP_57(eval_macro, tail_macro)
#define LEXP_59(eval_macro, tail_macro)                                        \
  eval_macro(58) LEXP_58(eval_macro, tail_macro)
#define LEXP_60(eval_macro, tail_macro)                                        \
  eval_macro(59) LEXP_59(eval_macro, tail_macro)
#define LEXP_61(eval_macro, tail_macro)                                        \
  eval_macro(60) LEXP_60(eval_macro, tail_macro)
#define LEXP_62(eval_macro, tail_macro)                                        \
  eval_macro(61) LEXP_61(eval_macro, tail_macro)
#define LEXP_63(eval_macro, tail_macro)                                        \
  eval_macro(62) LEXP_62(eval_macro, tail_macro)
#define LEXP_64(eval_macro, tail_macro)                                        \
  eval_macro(63) LEXP_63(eval_macro, tail_macro)
#define LEXP_65(eval_macro, tail_macro)                                        \
  eval_macro(64) LEXP_64(eval_macro, tail_macro)
#define LEXP_66(eval_macro, tail_macro)                                        \
  eval_macro(65) LEXP_65(eval_macro, tail_macro)
#define LEXP_67(eval_macro, tail_macro)                                        \
  eval_macro(66) LEXP_66(eval_macro, tail_macro)
#define LEXP_68(eval_macro, tail_macro)                                        \
  eval_macro(67) LEXP_67(eval_macro, tail_macro)
#define LEXP_69(eval_macro, tail_macro)                                        \
  eval_macro(68) LEXP_68(eval_macro, tail_macro)
#define LEXP_70(eval_macro, tail_macro)                                        \
  eval_macro(69) LEXP_69(eval_macro, tail_macro)
#define LEXP_71(eval_macro, tail_macro)                                        \
  eval_macro(70) LEXP_70(eval_macro, tail_macro)
#define LEXP_72(eval_macro, tail_macro)                                        \
  eval_macro(71) LEXP_71(eval_macro, tail_macro)
#define LEXP_73(eval_macro, tail_macro)                                        \
  eval_macro(72) LEXP_72(eval_macro, tail_macro)
#define LEXP_74(eval_macro, tail_macro)                                        \
  eval_macro(73) LEXP_73(eval_macro, tail_macro)
#define LEXP_75(eval_macro, tail_macro)                                        \
  eval_macro(74) LEXP_74(eval_macro, tail_macro)
#define LEXP_76(eval_macro, tail_macro)                                        \
  eval_macro(75) LEXP_75(eval_macro, tail_macro)
#define LEXP_77(eval_macro, tail_macro)                                        \
  eval_macro(76) LEXP_76(eval_macro, tail_macro)
#define LEXP_78(eval_macro, tail_macro)                                        \
  eval_macro(77) LEXP_77(eval_macro, tail_macro)
#define LEXP_79(eval_macro, tail_macro)                                        \
  eval_macro(78) LEXP_78(eval_macro, tail_macro)
#define LEXP_80(eval_macro, tail_macro)                                        \
  eval_macro(79) LEXP_79(eval_macro, tail_macro)
#define LEXP_81(eval_macro, tail_macro)                                        \
  eval_macro(80) LEXP_80(eval_macro, tail_macro)
#define LEXP_82(eval_macro, tail_macro)                                        \
  eval_macro(81) LEXP_81(eval_macro, tail_macro)
#define LEXP_83(eval_macro, tail_macro)                                        \
  eval_macro(82) LEXP_82(eval_macro, tail_macro)
#define LEXP_84(eval_macro, tail_macro)                                        \
  eval_macro(83) LEXP_83(eval_macro, tail_macro)
#define LEXP_85(eval_macro, tail_macro)                                        \
  eval_macro(84) LEXP_84(eval_macro, tail_macro)
#define LEXP_86(eval_macro, tail_macro)                                        \
  eval_macro(85) LEXP_85(eval_macro, tail_macro)
#define LEXP_87(eval_macro, tail_macro)                                        \
  eval_macro(86) LEXP_86(eval_macro, tail_macro)
#define LEXP_88(eval_macro, tail_macro)                                        \
  eval_macro(87) LEXP_87(eval_macro, tail_macro)
#define LEXP_89(eval_macro, tail_macro)                                        \
  eval_macro(88) LEXP_88(eval_macro, tail_macro)
#define LEXP_90(eval_macro, tail_macro)                                        \
  eval_macro(89) LEXP_89(eval_macro, tail_macro)
#define LEXP_91(eval_macro, tail_macro)                                        \
  eval_macro(90) LEXP_90(eval_macro, tail_macro)
#define LEXP_92(eval_macro, tail_macro)                                        \
  eval_macro(91) LEXP_91(eval_macro, tail_macro)
#define LEXP_93(eval_macro, tail_macro)                                        \
  eval_macro(92) LEXP_92(eval_macro, tail_macro)
#define LEXP_94(eval_macro, tail_macro)                                        \
  eval_macro(93) LEXP_93(eval_macro, tail_macro)
#define LEXP_95(eval_macro, tail_macro)                                        \
  eval_macro(94) LEXP_94(eval_macro, tail_macro)
#define LEXP_96(eval_macro, tail_macro)                                        \
  eval_macro(95) LEXP_95(eval_macro, tail_macro)
#define LEXP_97(eval_macro, tail_macro)                                        \
  eval_macro(96) LEXP_96(eval_macro, tail_macro)
#define LEXP_98(eval_macro, tail_macro)                                        \
  eval_macro(97) LEXP_97(eval_macro, tail_macro)
#define LEXP_99(eval_macro, tail_macro)                                        \
  eval_macro(98) LEXP_98(eval_macro, tail_macro)
#define LEXP_100(eval_macro, tail_macro)                                       \
  eval_macro(99) LEXP_99(eval_macro, tail_macro)
#define LEXP_101(eval_macro, tail_macro)                                       \
  eval_macro(100) LEXP_100(eval_macro, tail_macro)
#define LEXP_102(eval_macro, tail_macro)                                       \
  eval_macro(101) LEXP_101(eval_macro, tail_macro)
#define LEXP_103(eval_macro, tail_macro)                                       \
  eval_macro(102) LEXP_102(eval_macro, tail_macro)
#define LEXP_104(eval_macro, tail_macro)                                       \
  eval_macro(103) LEXP_103(eval_macro, tail_macro)
#define LEXP_105(eval_macro, tail_macro)                                       \
  eval_macro(104) LEXP_104(eval_macro, tail_macro)
#define LEXP_106(eval_macro, tail_macro)                                       \
  eval_macro(105) LEXP_105(eval_macro, tail_macro)
#define LEXP_107(eval_macro, tail_macro)                                       \
  eval_macro(106) LEXP_106(eval_macro, tail_macro)
#define LEXP_108(eval_macro, tail_macro)                                       \
  eval_macro(107) LEXP_107(eval_macro, tail_macro)
#define LEXP_109(eval_macro, tail_macro)                                       \
  eval_macro(108) LEXP_108(eval_macro, tail_macro)
#define LEXP_110(eval_macro, tail_macro)                                       \
  eval_macro(109) LEXP_109(eval_macro, tail_macro)
#define LEXP_111(eval_macro, tail_macro)                                       \
  eval_macro(110) LEXP_110(eval_macro, tail_macro)
#define LEXP_112(eval_macro, tail_macro)                                       \
  eval_macro(111) LEXP_111(eval_macro, tail_macro)
#define LEXP_113(eval_macro, tail_macro)                                       \
  eval_macro(112) LEXP_112(eval_macro, tail_macro)
#define LEXP_114(eval_macro, tail_macro)                                       \
  eval_macro(113) LEXP_113(eval_macro, tail_macro)
#define LEXP_115(eval_macro, tail_macro)                                       \
  eval_macro(114) LEXP_114(eval_macro, tail_macro)
#define LEXP_116(eval_macro, tail_macro)                                       \
  eval_macro(115) LEXP_115(eval_macro, tail_macro)
#define LEXP_117(eval_macro, tail_macro)                                       \
  eval_macro(116) LEXP_116(eval_macro, tail_macro)
#define LEXP_118(eval_macro, tail_macro)                                       \
  eval_macro(117) LEXP_117(eval_macro, tail_macro)
#define LEXP_119(eval_macro, tail_macro)                                       \
  eval_macro(118) LEXP_118(eval_macro, tail_macro)
#define LEXP_120(eval_macro, tail_macro)                                       \
  eval_macro(119) LEXP_119(eval_macro, tail_macro)
#define LEXP_121(eval_macro, tail_macro)                                       \
  eval_macro(120) LEXP_120(eval_macro, tail_macro)
#define LEXP_122(eval_macro, tail_macro)                                       \
  eval_macro(121) LEXP_121(eval_macro, tail_macro)
#define LEXP_123(eval_macro, tail_macro)                                       \
  eval_macro(122) LEXP_122(eval_macro, tail_macro)
#define LEXP_124(eval_macro, tail_macro)                                       \
  eval_macro(123) LEXP_123(eval_macro, tail_macro)
#define LEXP_125(eval_macro, tail_macro)                                       \
  eval_macro(124) LEXP_124(eval_macro, tail_macro)
#define LEXP_126(eval_macro, tail_macro)                                       \
  eval_macro(125) LEXP_125(eval_macro, tail_macro)
#define LEXP_127(eval_macro, tail_macro)                                       \
  eval_macro(126) LEXP_126(eval_macro, tail_macro)
#define LEXP_128(eval_macro, tail_macro)                                       \
  eval_macro(127) LEXP_127(eval_macro, tail_macro)
#define LEXP_129(eval_macro, tail_macro)                                       \
  eval_macro(128) LEXP_128(eval_macro, tail_macro)
#define LEXP_130(eval_macro, tail_macro)                                       \
  eval_macro(129) LEXP_129(eval_macro, tail_macro)
#define LEXP_131(eval_macro, tail_macro)                                       \
  eval_macro(130) LEXP_130(eval_macro, tail_macro)
#define LEXP_132(eval_macro, tail_macro)                                       \
  eval_macro(131) LEXP_131(eval_macro, tail_macro)
#define LEXP_133(eval_macro, tail_macro)                                       \
  eval_macro(132) LEXP_132(eval_macro, tail_macro)
#define LEXP_134(eval_macro, tail_macro)                                       \
  eval_macro(133) LEXP_133(eval_macro, tail_macro)
#define LEXP_135(eval_macro, tail_macro)                                       \
  eval_macro(134) LEXP_134(eval_macro, tail_macro)
#define LEXP_136(eval_macro, tail_macro)                                       \
  eval_macro(135) LEXP_135(eval_macro, tail_macro)
#define LEXP_137(eval_macro, tail_macro)                                       \
  eval_macro(136) LEXP_136(eval_macro, tail_macro)
#define LEXP_138(eval_macro, tail_macro)                                       \
  eval_macro(137) LEXP_137(eval_macro, tail_macro)
#define LEXP_139(eval_macro, tail_macro)                                       \
  eval_macro(138) LEXP_138(eval_macro, tail_macro)
#define LEXP_140(eval_macro, tail_macro)                                       \
  eval_macro(139) LEXP_139(eval_macro, tail_macro)
#define LEXP_141(eval_macro, tail_macro)                                       \
  eval_macro(140) LEXP_140(eval_macro, tail_macro)
#define LEXP_142(eval_macro, tail_macro)                                       \
  eval_macro(141) LEXP_141(eval_macro, tail_macro)
#define LEXP_143(eval_macro, tail_macro)                                       \
  eval_macro(142) LEXP_142(eval_macro, tail_macro)
#define LEXP_144(eval_macro, tail_macro)                                       \
  eval_macro(143) LEXP_143(eval_macro, tail_macro)
#define LEXP_145(eval_macro, tail_macro)                                       \
  eval_macro(144) LEXP_144(eval_macro, tail_macro)
#define LEXP_146(eval_macro, tail_macro)                                       \
  eval_macro(145) LEXP_145(eval_macro, tail_macro)
#define LEXP_147(eval_macro, tail_macro)                                       \
  eval_macro(146) LEXP_146(eval_macro, tail_macro)
#define LEXP_148(eval_macro, tail_macro)                                       \
  eval_macro(147) LEXP_147(eval_macro, tail_macro)
#define LEXP_149(eval_macro, tail_macro)                                       \
  eval_macro(148) LEXP_148(eval_macro, tail_macro)
#define LEXP_150(eval_macro, tail_macro)                                       \
  eval_macro(149) LEXP_149(eval_macro, tail_macro)
#define LEXP_151(eval_macro, tail_macro)                                       \
  eval_macro(150) LEXP_150(eval_macro, tail_macro)
#define LEXP_152(eval_macro, tail_macro)                                       \
  eval_macro(151) LEXP_151(eval_macro, tail_macro)
#define LEXP_153(eval_macro, tail_macro)                                       \
  eval_macro(152) LEXP_152(eval_macro, tail_macro)
#define LEXP_154(eval_macro, tail_macro)                                       \
  eval_macro(153) LEXP_153(eval_macro, tail_macro)
#define LEXP_155(eval_macro, tail_macro)                                       \
  eval_macro(154) LEXP_154(eval_macro, tail_macro)
#define LEXP_156(eval_macro, tail_macro)                                       \
  eval_macro(155) LEXP_155(eval_macro, tail_macro)
#define LEXP_157(eval_macro, tail_macro)                                       \
  eval_macro(156) LEXP_156(eval_macro, tail_macro)
#define LEXP_158(eval_macro, tail_macro)                                       \
  eval_macro(157) LEXP_157(eval_macro, tail_macro)
#define LEXP_159(eval_macro, tail_macro)                                       \
  eval_macro(158) LEXP_158(eval_macro, tail_macro)
#define LEXP_160(eval_macro, tail_macro)                                       \
  eval_macro(159) LEXP_159(eval_macro, tail_macro)
#define LEXP_161(eval_macro, tail_macro)                                       \
  eval_macro(160) LEXP_160(eval_macro, tail_macro)
#define LEXP_162(eval_macro, tail_macro)                                       \
  eval_macro(161) LEXP_161(eval_macro, tail_macro)
#define LEXP_163(eval_macro, tail_macro)                                       \
  eval_macro(162) LEXP_162(eval_macro, tail_macro)
#define LEXP_164(eval_macro, tail_macro)                                       \
  eval_macro(163) LEXP_163(eval_macro, tail_macro)
#define LEXP_165(eval_macro, tail_macro)                                       \
  eval_macro(164) LEXP_164(eval_macro, tail_macro)
#define LEXP_166(eval_macro, tail_macro)                                       \
  eval_macro(165) LEXP_165(eval_macro, tail_macro)
#define LEXP_167(eval_macro, tail_macro)                                       \
  eval_macro(166) LEXP_166(eval_macro, tail_macro)
#define LEXP_168(eval_macro, tail_macro)                                       \
  eval_macro(167) LEXP_167(eval_macro, tail_macro)
#define LEXP_169(eval_macro, tail_macro)                                       \
  eval_macro(168) LEXP_168(eval_macro, tail_macro)
#define LEXP_170(eval_macro, tail_macro)                                       \
  eval_macro(169) LEXP_169(eval_macro, tail_macro)
#define LEXP_171(eval_macro, tail_macro)                                       \
  eval_macro(170) LEXP_170(eval_macro, tail_macro)
#define LEXP_172(eval_macro, tail_macro)                                       \
  eval_macro(171) LEXP_171(eval_macro, tail_macro)
#define LEXP_173(eval_macro, tail_macro)                                       \
  eval_macro(172) LEXP_172(eval_macro, tail_macro)
#define LEXP_174(eval_macro, tail_macro)                                       \
  eval_macro(173) LEXP_173(eval_macro, tail_macro)
#define LEXP_175(eval_macro, tail_macro)                                       \
  eval_macro(174) LEXP_174(eval_macro, tail_macro)
#define LEXP_176(eval_macro, tail_macro)                                       \
  eval_macro(175) LEXP_175(eval_macro, tail_macro)
#define LEXP_177(eval_macro, tail_macro)                                       \
  eval_macro(176) LEXP_176(eval_macro, tail_macro)
#define LEXP_178(eval_macro, tail_macro)                                       \
  eval_macro(177) LEXP_177(eval_macro, tail_macro)
#define LEXP_179(eval_macro, tail_macro)                                       \
  eval_macro(178) LEXP_178(eval_macro, tail_macro)
#define LEXP_180(eval_macro, tail_macro)                                       \
  eval_macro(179) LEXP_179(eval_macro, tail_macro)
#define LEXP_181(eval_macro, tail_macro)                                       \
  eval_macro(180) LEXP_180(eval_macro, tail_macro)
#define LEXP_182(eval_macro, tail_macro)                                       \
  eval_macro(181) LEXP_181(eval_macro, tail_macro)
#define LEXP_183(eval_macro, tail_macro)                                       \
  eval_macro(182) LEXP_182(eval_macro, tail_macro)
#define LEXP_184(eval_macro, tail_macro)                                       \
  eval_macro(183) LEXP_183(eval_macro, tail_macro)
#define LEXP_185(eval_macro, tail_macro)                                       \
  eval_macro(184) LEXP_184(eval_macro, tail_macro)
#define LEXP_186(eval_macro, tail_macro)                                       \
  eval_macro(185) LEXP_185(eval_macro, tail_macro)
#define LEXP_187(eval_macro, tail_macro)                                       \
  eval_macro(186) LEXP_186(eval_macro, tail_macro)
#define LEXP_188(eval_macro, tail_macro)                                       \
  eval_macro(187) LEXP_187(eval_macro, tail_macro)
#define LEXP_189(eval_macro, tail_macro)                                       \
  eval_macro(188) LEXP_188(eval_macro, tail_macro)
#define LEXP_190(eval_macro, tail_macro)                                       \
  eval_macro(189) LEXP_189(eval_macro, tail_macro)
#define LEXP_191(eval_macro, tail_macro)                                       \
  eval_macro(190) LEXP_190(eval_macro, tail_macro)
#define LEXP_192(eval_macro, tail_macro)                                       \
  eval_macro(191) LEXP_191(eval_macro, tail_macro)
#define LEXP_193(eval_macro, tail_macro)                                       \
  eval_macro(192) LEXP_192(eval_macro, tail_macro)
#define LEXP_194(eval_macro, tail_macro)                                       \
  eval_macro(193) LEXP_193(eval_macro, tail_macro)
#define LEXP_195(eval_macro, tail_macro)                                       \
  eval_macro(194) LEXP_194(eval_macro, tail_macro)
#define LEXP_196(eval_macro, tail_macro)                                       \
  eval_macro(195) LEXP_195(eval_macro, tail_macro)
#define LEXP_197(eval_macro, tail_macro)                                       \
  eval_macro(196) LEXP_196(eval_macro, tail_macro)
#define LEXP_198(eval_macro, tail_macro)                                       \
  eval_macro(197) LEXP_197(eval_macro, tail_macro)
#define LEXP_199(eval_macro, tail_macro)                                       \
  eval_macro(198) LEXP_198(eval_macro, tail_macro)
#define LEXP_200(eval_macro, tail_macro)                                       \
  eval_macro(199) LEXP_199(eval_macro, tail_macro)
#define LEXP_201(eval_macro, tail_macro)                                       \
  eval_macro(200) LEXP_200(eval_macro, tail_macro)
#define LEXP_202(eval_macro, tail_macro)                                       \
  eval_macro(201) LEXP_201(eval_macro, tail_macro)
#define LEXP_203(eval_macro, tail_macro)                                       \
  eval_macro(202) LEXP_202(eval_macro, tail_macro)
#define LEXP_204(eval_macro, tail_macro)                                       \
  eval_macro(203) LEXP_203(eval_macro, tail_macro)
#define LEXP_205(eval_macro, tail_macro)                                       \
  eval_macro(204) LEXP_204(eval_macro, tail_macro)
#define LEXP_206(eval_macro, tail_macro)                                       \
  eval_macro(205) LEXP_205(eval_macro, tail_macro)
#define LEXP_207(eval_macro, tail_macro)                                       \
  eval_macro(206) LEXP_206(eval_macro, tail_macro)
#define LEXP_208(eval_macro, tail_macro)                                       \
  eval_macro(207) LEXP_207(eval_macro, tail_macro)
#define LEXP_209(eval_macro, tail_macro)                                       \
  eval_macro(208) LEXP_208(eval_macro, tail_macro)
#define LEXP_210(eval_macro, tail_macro)                                       \
  eval_macro(209) LEXP_209(eval_macro, tail_macro)
#define LEXP_211(eval_macro, tail_macro)                                       \
  eval_macro(210) LEXP_210(eval_macro, tail_macro)
#define LEXP_212(eval_macro, tail_macro)                                       \
  eval_macro(211) LEXP_211(eval_macro, tail_macro)
#define LEXP_213(eval_macro, tail_macro)                                       \
  eval_macro(212) LEXP_212(eval_macro, tail_macro)
#define LEXP_214(eval_macro, tail_macro)                                       \
  eval_macro(213) LEXP_213(eval_macro, tail_macro)
#define LEXP_215(eval_macro, tail_macro)                                       \
  eval_macro(214) LEXP_214(eval_macro, tail_macro)
#define LEXP_216(eval_macro, tail_macro)                                       \
  eval_macro(215) LEXP_215(eval_macro, tail_macro)
#define LEXP_217(eval_macro, tail_macro)                                       \
  eval_macro(216) LEXP_216(eval_macro, tail_macro)
#define LEXP_218(eval_macro, tail_macro)                                       \
  eval_macro(217) LEXP_217(eval_macro, tail_macro)
#define LEXP_219(eval_macro, tail_macro)                                       \
  eval_macro(218) LEXP_218(eval_macro, tail_macro)
#define LEXP_220(eval_macro, tail_macro)                                       \
  eval_macro(219) LEXP_219(eval_macro, tail_macro)
#define LEXP_221(eval_macro, tail_macro)                                       \
  eval_macro(220) LEXP_220(eval_macro, tail_macro)
#define LEXP_222(eval_macro, tail_macro)                                       \
  eval_macro(221) LEXP_221(eval_macro, tail_macro)
#define LEXP_223(eval_macro, tail_macro)                                       \
  eval_macro(222) LEXP_222(eval_macro, tail_macro)
#define LEXP_224(eval_macro, tail_macro)                                       \
  eval_macro(223) LEXP_223(eval_macro, tail_macro)
#define LEXP_225(eval_macro, tail_macro)                                       \
  eval_macro(224) LEXP_224(eval_macro, tail_macro)
#define LEXP_226(eval_macro, tail_macro)                                       \
  eval_macro(225) LEXP_225(eval_macro, tail_macro)
#define LEXP_227(eval_macro, tail_macro)                                       \
  eval_macro(226) LEXP_226(eval_macro, tail_macro)
#define LEXP_228(eval_macro, tail_macro)                                       \
  eval_macro(227) LEXP_227(eval_macro, tail_macro)
#define LEXP_229(eval_macro, tail_macro)                                       \
  eval_macro(228) LEXP_228(eval_macro, tail_macro)
#define LEXP_230(eval_macro, tail_macro)                                       \
  eval_macro(229) LEXP_229(eval_macro, tail_macro)
#define LEXP_231(eval_macro, tail_macro)                                       \
  eval_macro(230) LEXP_230(eval_macro, tail_macro)
#define LEXP_232(eval_macro, tail_macro)                                       \
  eval_macro(231) LEXP_231(eval_macro, tail_macro)
#define LEXP_233(eval_macro, tail_macro)                                       \
  eval_macro(232) LEXP_232(eval_macro, tail_macro)
#define LEXP_234(eval_macro, tail_macro)                                       \
  eval_macro(233) LEXP_233(eval_macro, tail_macro)
#define LEXP_235(eval_macro, tail_macro)                                       \
  eval_macro(234) LEXP_234(eval_macro, tail_macro)
#define LEXP_236(eval_macro, tail_macro)                                       \
  eval_macro(235) LEXP_235(eval_macro, tail_macro)
#define LEXP_237(eval_macro, tail_macro)                                       \
  eval_macro(236) LEXP_236(eval_macro, tail_macro)
#define LEXP_238(eval_macro, tail_macro)                                       \
  eval_macro(237) LEXP_237(eval_macro, tail_macro)
#define LEXP_239(eval_macro, tail_macro)                                       \
  eval_macro(238) LEXP_238(eval_macro, tail_macro)
#define LEXP_240(eval_macro, tail_macro)                                       \
  eval_macro(239) LEXP_239(eval_macro, tail_macro)
#define LEXP_241(eval_macro, tail_macro)                                       \
  eval_macro(240) LEXP_240(eval_macro, tail_macro)
#define LEXP_242(eval_macro, tail_macro)                                       \
  eval_macro(241) LEXP_241(eval_macro, tail_macro)
#define LEXP_243(eval_macro, tail_macro)                                       \
  eval_macro(242) LEXP_242(eval_macro, tail_macro)
#define LEXP_244(eval_macro, tail_macro)                                       \
  eval_macro(243) LEXP_243(eval_macro, tail_macro)
#define LEXP_245(eval_macro, tail_macro)                                       \
  eval_macro(244) LEXP_244(eval_macro, tail_macro)
#define LEXP_246(eval_macro, tail_macro)                                       \
  eval_macro(245) LEXP_245(eval_macro, tail_macro)
#define LEXP_247(eval_macro, tail_macro)                                       \
  eval_macro(246) LEXP_246(eval_macro, tail_macro)
#define LEXP_248(eval_macro, tail_macro)                                       \
  eval_macro(247) LEXP_247(eval_macro, tail_macro)
#define LEXP_249(eval_macro, tail_macro)                                       \
  eval_macro(248) LEXP_248(eval_macro, tail_macro)
#define LEXP_250(eval_macro, tail_macro)                                       \
  eval_macro(249) LEXP_249(eval_macro, tail_macro)
#define LEXP_251(eval_macro, tail_macro)                                       \
  eval_macro(250) LEXP_250(eval_macro, tail_macro)
#define LEXP_252(eval_macro, tail_macro)                                       \
  eval_macro(251) LEXP_251(eval_macro, tail_macro)
#define LEXP_253(eval_macro, tail_macro)                                       \
  eval_macro(252) LEXP_252(eval_macro, tail_macro)
#define LEXP_254(eval_macro, tail_macro)                                       \
  eval_macro(253) LEXP_253(eval_macro, tail_macro)
#define LEXP_255(eval_macro, tail_macro)                                       \
  eval_macro(254) LEXP_254(eval_macro, tail_macro)
#define LEXP_256(eval_macro, tail_macro)                                       \
  eval_macro(255) LEXP_255(eval_macro, tail_macro)

#define LEXP(n, eval_macro, tail_macro) n(eval_macro, tail_macro)
#define _STR(x) LEXP_##x
#define STR(x) _STR(x)

kernel void spill_test(global const int *in, global int *out,
                       global const uint *offset) {
#define read_input(idx) int _data##idx = in[get_global_id(0) + offset[idx]];
#define sum_nonaff_data(idx) _data##idx *idx +
#define sum_nonaff_data_tail() 0

#define sum_sq_data(idx) _data##idx *_data##idx +
#define sum_sq_data_tail() 0

  LEXP(STR(SIZE), read_input, NO_TAIL)
  out[get_global_id(0) * 2] =
      LEXP(STR(SIZE), sum_nonaff_data, sum_nonaff_data_tail);
  out[get_global_id(0) * 2 + 1] =
      LEXP(STR(SIZE), sum_sq_data, sum_sq_data_tail);
}