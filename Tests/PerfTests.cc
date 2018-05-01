//
// PerfTests.cc
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

#include "FleeceTests.hh"
#include "Fleece.hh"
#include "JSONConverter.hh"
#include "varint.hh"
#include <chrono>
#include <thread>
#ifndef _MSC_VER
#include <unistd.h>
#endif

// Catch's REQUIRE is too slow for perf testing
#undef REQUIRE
#define REQUIRE(TEST)   while (!(TEST)) {abort();}
#undef CHECK
#define CHECK(TEST)     while (!(TEST)) {abort();}
#ifdef __APPLE__
#define FLEECE_UNUSED __unused
#elif !defined(_MSC_VER)
#define FLEECE_UNUSED __attribute__((unused))
#else
#define FLEECE_UNUSED
#endif


using namespace fleece;

static const bool kSortKeys = true;

TEST_CASE("GetUVarint performance", "[.Perf]") {
    static constexpr int kNRounds = 10000000;
    Benchmark bench;
    uint8_t buf[100];
    fprintf(stderr, "buf = %p\n", &buf);
    double d = 1.0;
    while (d <= UINT64_MAX) {
        auto n = (uint64_t)d;
        size_t nBytes = PutUVarInt(buf, n);
        uint64_t result = 0;
        bench.start();
        for (int round = 0; round < kNRounds; ++round) {
            uint64_t nn;
            CHECK(GetUVarInt(slice(buf, sizeof(buf)), &nn) == nBytes);
            result += nn;
        }
        bench.stop();
        CHECK(result != 1); // bogus
        fprintf(stderr, "n = %16llx; %2zd bytes; time = %.3f ns\n",
                n, nBytes,
                bench.elapsed() / kNRounds * 1.0e9);
         d *= 1.5;
    }
    bench.printReport(1.0/kNRounds);
}

TEST_CASE("Perf Convert1000People", "[.Perf]") {
    static const int kSamples = 500;

    std::vector<double> elapsedTimes;
    alloc_slice input = readFile(kTestFilesDir "1000people.json");

    Benchmark bench;

    alloc_slice lastResult;
    fprintf(stderr, "Converting JSON to Fleece...\n");
    for (int i = 0; i < kSamples; i++) {
        bench.start();
        {
            Encoder e(input.size);
            e.uniqueStrings(true);
            e.sortKeys(kSortKeys);
            JSONConverter jr(e);

            jr.encodeJSON(input);
            e.end();
            auto result = e.extractOutput();
            if (i == kSamples-1)
                lastResult = result;
        }
        bench.stop();

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    bench.printReport();

    fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
            input.size, lastResult.size, (lastResult.size*100.0/input.size));
    writeToFile(lastResult, kTestFilesDir "1000people.fleece");
}

TEST_CASE("Perf LoadFleece", "[.Perf]") {
    static const int kIterations = 1000;
    alloc_slice doc = readFile(kTestFilesDir "1000people.fleece");

    {
        fprintf(stderr, "Scanning untrusted Fleece... ");
        Benchmark bench;
        for (int i = 0; i < kIterations; i++) {
            bench.start();
            FLEECE_UNUSED auto root = Value::fromData(doc)->asArray();
            REQUIRE(root != nullptr);
            bench.stop();
        }
        bench.printReport();
    }

    {
        fprintf(stderr, "Scanning trusted Fleece... ");
        static const int kIterationsPerSample = 1000000;
        Benchmark bench;
        for (int i = 0; i < kIterations; i++) {
            bench.start();
            for (int j = 0; j < kIterationsPerSample; j++) {
                FLEECE_UNUSED auto root = Value::fromTrustedData(doc)->asArray();
                REQUIRE(root != nullptr);
            }
            bench.stop();
        }
        bench.printReport(1.0/kIterationsPerSample);
    }
}

static void testFindPersonByIndex(int sort) {
    int kSamples = 500;
    int kIterations = 10000;
    Benchmark bench;

    mmap_slice doc(kTestFilesDir "1000people.fleece");

    Dict::key nameKey(slice("name"));

    fprintf(stderr, "Looking up one value, sorted=%d...\n", sort);
    for (int i = 0; i < kSamples; i++) {
        bench.start();

        for (int j = 0; j < kIterations; j++) {
            auto root = Value::fromTrustedData(doc)->asArray();
            auto person = root->get(123)->asDict();
            const Value *name;
            switch (sort) {
                case 0: name = person->get_unsorted(slice("name")); break;
                case 1: name = person->get(slice("name")); break;
                case 2: name = person->get(nameKey); break;
                default: return;
            }
            std::string nameStr = (std::string)name->asString();
#ifndef NDEBUG
            REQUIRE(nameStr == std::string("Concepcion Burns"));
#endif
        }

        bench.stop();
    }
    bench.printReport(1.0/kIterations);
}

TEST_CASE("Perf FindPersonByIndexUnsorted", "[.Perf]")    {testFindPersonByIndex(0);}
TEST_CASE("Perf FindPersonByIndexSorted", "[.Perf]")      {if (kSortKeys) testFindPersonByIndex(1);}
TEST_CASE("Perf FindPersonByIndexKeyed", "[.Perf]")       {testFindPersonByIndex(2);}

static void testLoadPeople(bool multiKeyGet) {
    int kSamples = 50;
    int kIterations = 1000;
    Benchmark bench;

    mmap_slice doc(kTestFilesDir "1000people.fleece");

    Dict::key keys[10] = {
        Dict::key(slice("about")),
        Dict::key(slice("age")),
        Dict::key(slice("balance")),
        Dict::key(slice("guid")),
        Dict::key(slice("isActive")),
        Dict::key(slice("latitude")),
        Dict::key(slice("longitude")),
        Dict::key(slice("name")),
        Dict::key(slice("registered")),
        Dict::key(slice("tags")),
    };
    Dict::sortKeys(keys, 10);

    fprintf(stderr, "Looking up 1000 people, multi-key get=%d...\n", multiKeyGet);
    for (int i = 0; i < kSamples; i++) {
        bench.start();

        for (int j = 0; j < kIterations; j++) {
            auto root = Value::fromTrustedData(doc)->asArray();
            for (Array::iterator iter(root); iter; ++iter) {
                const Dict *person = iter->asDict();
                size_t n = 0;
                if (multiKeyGet) {
                    const Value* values[10];
                    n = person->get(keys, values, 10);
                } else {
                    for (int k = 0; k < 10; k++)
                        if (person->get(keys[k]) != nullptr)
                            n++;
                }
                REQUIRE(n == 10);
            }
        }

        bench.stop();
    }
    bench.printReport(1.0/kIterations, "person");
}

TEST_CASE("Perf LoadPeople", "[.Perf]") {testLoadPeople(false);}
TEST_CASE("Perf LoadPeopleFast", "[.Perf]") {testLoadPeople(true);}
