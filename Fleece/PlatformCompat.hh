//
// PlatformCompat.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once


#ifdef _MSC_VER

    #define _usuallyTrue(VAL)               (VAL)
    #define _usuallyFalse(VAL)              (VAL)
    #define NOINLINE                        __declspec(noinline)
	#define LITECORE_UNUSED
    #define NONNULL
    #define __typeof                        decltype

    #define __has_extension(X)              0
    #define __has_feature(F)                0
    #define __func__                        __FUNCTION__

    #define localtime_r(a, b)               localtime_s(b, a)
    #define timegm                          _mkgmtime

    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;

    #define MAXFLOAT FLT_MAX

    #define __printflike(A, B) 

    #include <winapifamily.h>

#else

    #ifdef __APPLE__
    #define LITECORE_UNUSED __unused
    #else
    #define LITECORE_UNUSED __attribute__((unused))
    #endif

    #define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
    #define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
    #define NOINLINE                        __attribute((noinline))
    #define NONNULL                         __attribute__((nonnull))

    #ifndef __printflike
    #define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif

    #define WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) 0

#endif
