//
// FleeceTests.hh
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#include "slice.hh"
#include "JSON5.hh"
#include "Benchmark.hh"
#include <ostream>
#ifdef _MSC_VER
#include <windows.h>
#endif

using namespace fleece;

// Directory containing test files:
#ifdef _MSC_VER
#define kTestFilesDir "..\\Tests\\"
#else
#define kTestFilesDir "Tests/"
#endif

namespace fleece_test {
    std::string sliceToHex(slice);
    std::string sliceToHexDump(slice, size_t width = 16);
    std::ostream& dumpSlice(std::ostream&, slice);

    alloc_slice readFile(const char *path);
    void writeToFile(slice s, const char *path);


    struct mmap_slice : public pure_slice {
        mmap_slice(const char *path);
        ~mmap_slice();

        operator slice()    {return {buf, size};}

    private:
        void* _mapped;
    #ifdef _MSC_VER
        HANDLE _fileHandle{INVALID_HANDLE_VALUE};
        HANDLE _mapHandle{INVALID_HANDLE_VALUE};
    #else
        int _fd;
    #endif
        mmap_slice(const mmap_slice&);
    };

    // Converts JSON5 to JSON; helps make JSON test input more readable!
    static inline std::string json5(const std::string &s)      {return fleece::ConvertJSON5(s);}
}

using namespace fleece_test;

namespace fleece {
    // to make slice work with Catch's logging. This has to be in the 'fleece' namespace.
    static inline std::ostream& operator<< (std::ostream& o, slice s) {
        return dumpSlice(o, s);
    }
}

// This has to come last so that '<<' overrides can be used by Catch.
#include "catch.hpp"
