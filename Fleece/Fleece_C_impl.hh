//
// Fleece_C_impl.hh
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
#include "Fleece.hh"
#include "JSONEncoder.hh"
#include "Path.hh"
#include "DeepIterator.hh"
#include "FleeceException.hh"
using namespace fleece;

namespace fleece {
    struct FLEncoderImpl;
}

#define FL_IMPL
typedef const Value* FLValue;
typedef const Array* FLArray;
typedef const Dict* FLDict;
typedef FLEncoderImpl* FLEncoder;
typedef SharedKeys* FLSharedKeys;
typedef Path*       FLKeyPath;
typedef DeepIterator* FLDeepIterator;

#include "Fleece.h" /* the C header */


namespace fleece {

    void recordError(const std::exception &x, FLError *outError) noexcept;

    #define catchError(OUTERROR) \
        catch (const std::exception &x) { recordError(x, OUTERROR); }

    
    // Implementation of FLEncoder: a subclass of Encoder that keeps track of its error state.
    struct FLEncoderImpl {
        FLError errorCode {::kFLNoError};
        const bool ownsFleeceEncoder {true};
        std::string errorMessage;
        std::unique_ptr<Encoder> fleeceEncoder;
        std::unique_ptr<JSONEncoder> jsonEncoder;
        std::unique_ptr<JSONConverter> jsonConverter;
        void* extraInfo {nullptr};

        FLEncoderImpl(FLEncoderFormat format,
                      size_t reserveSize =0, bool uniqueStrings =true, bool sortKeys =true)
        {
            if (reserveSize == 0)
                reserveSize = 256;
            if (format == kFLEncodeFleece) {
                fleeceEncoder.reset(new Encoder(reserveSize));
                fleeceEncoder->uniqueStrings(uniqueStrings);
                fleeceEncoder->sortKeys(sortKeys);
            } else {
                jsonEncoder.reset(new JSONEncoder(reserveSize));
                jsonEncoder->setJSON5(format == kFLEncodeJSON5);
            }
        }

        FLEncoderImpl(Encoder *encoder)
        :ownsFleeceEncoder(false)
        ,fleeceEncoder(encoder)
        { }

        ~FLEncoderImpl() {
            if (!ownsFleeceEncoder)
                fleeceEncoder.release();
        }

        bool isFleece() const {
            return fleeceEncoder != nullptr;
        }

        bool hasError() const {
            return errorCode != ::NoError;
        }

        void recordException(const std::exception &x) noexcept {
            if (!hasError()) {
                fleece::recordError(x, &errorCode);
                errorMessage = x.what();
            }
        }

        void reset() {
            if (fleeceEncoder)
                fleeceEncoder->reset();
            if (jsonConverter)
                jsonConverter->reset();
            errorCode = ::kFLNoError;
            extraInfo = nullptr;
        }
    };

    #define ENCODER_DO(E, METHOD) \
        (E->isFleece() ? E->fleeceEncoder->METHOD : E->jsonEncoder->METHOD)

    // Body of an FLEncoder_WriteXXX function:
    #define ENCODER_TRY(E, METHOD) \
        try{ \
            if (!E->hasError()) { \
                ENCODER_DO(E, METHOD); \
                return true; \
            } \
        } catch (const std::exception &x) { \
            E->recordException(x); \
        } \
        return false;

}
