#pragma once
#include <stdint.h>



////////////////////////////////////////////////////////////////////////////////
/// Platform
////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
#  define _CXLIB_PLATFORM_WIN32 1
#endif

#if !defined(_CXLIB_PLATFORM_WIN32)
#  error !!
#endif

////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// Compiler
////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#  define _CXLIB_HAS_MSVC_COMPILER 1
#elif defined(__GNUC__) || defined(__GNUG__)
#  define _CXLIB_HAS_GCC_COMPILER 1
#elif defined(__clang__)
#  define _CXLIB_HAS_CLANG_COMPILER 1
#endif

#if !defined(_CXLIB_HAS_MSVC_COMPILER)
#  error !!
#endif

////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// CXX
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
#  if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#    define _CXLIB_CXX_VER _MSVC_LANG
#  else  // ^^^ language mode is _MSVC_LANG / language mode is __cplusplus vvv
#    define _CXLIB_CXX_VER __cplusplus
#  endif // ^^^ language mode is larger of _MSVC_LANG and __cplusplus ^^^
#else  // ^^^ determine compiler's C++ mode / no C++ support vvv
#  define _CXLIB_CXX_VER 0L
#endif // ^^^ no C++ support ^^^

#ifndef _CXLIB_HAS_CXX17
#  if _CXLIB_CXX_VER > 201402L
#    define _CXLIB_HAS_CXX17 1
#  else
#    define _CXLIB_HAS_CXX17 0
#  endif
#endif // _CXLIB_HAS_CXX17

#ifndef _CXLIB_HAS_CXX20
#  if _CXLIB_HAS_CXX17 && _CXLIB_CXX_VER > 201703L
#    define _CXLIB_HAS_CXX20 1
#  else
#    define _CXLIB_HAS_CXX20 0
#  endif
#endif // _CXLIB_HAS_CXX20

#ifndef _CXLIB_HAS_CXX23
#  if _CXLIB_HAS_CXX20 && _CXLIB_CXX_VER > 202002L
#    define _CXLIB_HAS_CXX23 1
#  else
#    define _CXLIB_HAS_CXX23 0
#  endif
#endif // _CXLIB_HAS_CXX23

#if _CXLIB_HAS_CXX20 && !_CXLIB_HAS_CXX17
#  error _CXLIB_HAS_CXX20 must imply _CXLIB_HAS_CXX17.
#endif

#if _CXLIB_HAS_CXX23 && !_CXLIB_HAS_CXX20
#  error _CXLIB_HAS_CXX23 must imply _CXLIB_HAS_CXX20.
#endif

////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// Utility
////////////////////////////////////////////////////////////////////////////////

/**
 * constexpr function (since C++11)
 * http://en.cppreference.com/w/cpp/language/constexpr
 *
 * the function body must be either `delete`ed or `default`ed,
 * or contain only the following: (until C++14)
 *
 *     - null statements (plain semicolons)
 *     - static_assert declarations
 *     - typedef declarations & alias declarations that do not define classes or enumerations (until C++14)
 *     - using declarations & directives (using <typename>, using namespace)
 *     - if the function is not a constructor, exactly ONE return statement
 */

 // Functions that became constexpr in C++17
#if _CXLIB_HAS_CXX17
#  define _CXLIB_CONSTEXPR17 constexpr
#else  // ^^^ constexpr in C++17 and later / inline (not constexpr) in C++14 vvv
#  define _CXLIB_CONSTEXPR17 inline
#endif // ^^^ inline (not constexpr) in C++14 ^^^

// Functions that became constexpr in C++20
#if _CXLIB_HAS_CXX20
#  define _CXLIB_CONSTEXPR20 constexpr
#else  // ^^^ constexpr in C++20 and later / inline (not constexpr) in C++17 and earlier vvv
#  define _CXLIB_CONSTEXPR20 inline
#endif // ^^^ inline (not constexpr) in C++17 and earlier ^^^

// Functions that became constexpr in C++23
#if _CXLIB_HAS_CXX23
#  define _CXLIB_CONSTEXPR23 constexpr
#else  // ^^^ constexpr in C++23 and later / inline (not constexpr) in C++20 and earlier vvv
#  define _CXLIB_CONSTEXPR23 inline
#endif // ^^^ inline (not constexpr) in C++20 and earlier ^^^

////////////////////////////////////////////////////////////////////////////////

// Concat two arguments.
#define __CXLIB_PP_CONCAT(X, Y) X ## Y
#define _CXLIB_PP_CONCAT(X, Y) __CXLIB_PP_CONCAT(X, Y)

// Expand argument.
#define _CXLIB_PP_EXPAND(X) X

// Returns the 100th argument.
#define _CXLIB_PP_ARG100(_,\
   _100,_99,_98,_97,_96,_95,_94,_93,_92,_91,_90,_89,_88,_87,_86,_85,_84,_83,_82,_81, \
   _80,_79,_78,_77,_76,_75,_74,_73,_72,_71,_70,_69,_68,_67,_66,_65,_64,_63,_62,_61, \
   _60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41, \
   _40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21, \
   _20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,...) _1

// Returns whether __VA_ARGS__ has a comma (up to 100 arguments).
// Note: MSVC does not expand __VA_ARGS__ like most other compilers, so an extra step(expansion) is necessary.
// Ref: https://stackoverflow.com/a/66556553
#define _CXLIB_PP_HAS_COMMA(...) _CXLIB_PP_EXPAND(_CXLIB_PP_ARG100(__VA_ARGS__, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0))

////////////////////////////////////////////////////////////////////////////////

#define _CXLIB_NAMESPACE_BEGIN namespace cxlib { 
#define _CXLIB_NAMESPACE_END } 
#define _CXLIB ::cxlib::

////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// Third-Party Library Dependency
////////////////////////////////////////////////////////////////////////////////

//#define _CXLIB_HAS_FMT  1
#define _CXLIB_HAS_SPDLOG 1