#pragma once

// Concat two arguments.
#define __TRIENGINE_PP_CONCAT(X, Y) X ## Y
#define _TRIENGINE_PP_CONCAT(X, Y) __TRIENGINE_PP_CONCAT(X, Y)

// Expand argument.
#define _TRIENGINE_PP_EXPAND(X) X

// Returns the 100th argument.
#define _TRIENGINE_PP_ARG100(_,\
   _100,_99,_98,_97,_96,_95,_94,_93,_92,_91,_90,_89,_88,_87,_86,_85,_84,_83,_82,_81, \
   _80,_79,_78,_77,_76,_75,_74,_73,_72,_71,_70,_69,_68,_67,_66,_65,_64,_63,_62,_61, \
   _60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41, \
   _40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21, \
   _20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,...) _1

// Returns whether __VA_ARGS__ has a comma (up to 100 arguments).
// Note: MSVC does not expand __VA_ARGS__ like most other compilers, so an extra step(expansion) is necessary.
// Ref: https://stackoverflow.com/a/66556553
#define _TRIENGINE_PP_HAS_COMMA(...) _TRIENGINE_PP_EXPAND(_TRIENGINE_PP_ARG100(__VA_ARGS__, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0))
