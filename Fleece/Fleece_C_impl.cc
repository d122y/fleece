//
// fleece_C_impl.cc
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

#include "Fleece_C_impl.hh"
#include "Fleece.h"
#include "JSON5.hh"


namespace fleece {

    void recordError(const std::exception &x, FLError *outError) noexcept {
        if (outError)
            *outError = (FLError) FleeceException::getCode(x);
    }

}


bool FLSlice_Equal(FLSlice a, FLSlice b)        {return (slice)a == (slice)b;}
int FLSlice_Compare(FLSlice a, FLSlice b)       {return ((slice)a).compare((slice)b); }


static FLSliceResult toSliceResult(alloc_slice &&s) {
    s.retain();
    return {(void*)s.buf, s.size};
}

void FLSliceResult_Free(FLSliceResult s) {
    alloc_slice::release({s.buf, s.size});
}


FLValue FLValue_FromData(FLSlice data)          {return Value::fromData(data);}
FLValue FLValue_FromTrustedData(FLSlice data)   {return Value::fromTrustedData(data);}


FLValueType FLValue_GetType(FLValue v)          {return v ? (FLValueType)v->type() : kFLUndefined;}
bool FLValue_IsInteger(FLValue v)               {return v && v->isInteger();}
bool FLValue_IsUnsigned(FLValue v)              {return v && v->isUnsigned();}
bool FLValue_IsDouble(FLValue v)                {return v && v->isDouble();}
bool FLValue_AsBool(FLValue v)                  {return v && v->asBool();}
int64_t FLValue_AsInt(FLValue v)                {return v ? v->asInt() : 0;}
uint64_t FLValue_AsUnsigned(FLValue v)          {return v ? v->asUnsigned() : 0;}
float FLValue_AsFloat(FLValue v)                {return v ? v->asFloat() : 0.0;}
double FLValue_AsDouble(FLValue v)              {return v ? v->asDouble() : 0.0;}
FLString FLValue_AsString(FLValue v)            {return v ? (FLString)v->asString() : kFLSliceNull;}
FLSlice FLValue_AsData(FLValue v)               {return v ? (FLSlice)v->asData() : kFLSliceNull;}
FLArray FLValue_AsArray(FLValue v)              {return v ? v->asArray() : nullptr;}
FLDict FLValue_AsDict(FLValue v)                {return v ? v->asDict() : nullptr;}


FLSliceResult FLValue_ToString(FLValue v) {
    if (v) {
        try {
            return toSliceResult(v->toString());    // toString can throw
        } catchError(nullptr)
    }
    return {nullptr, 0};
}


FLSliceResult FLValue_ToJSONX(FLValue v,
                              FLSharedKeys sk,
                              bool json5,
                              bool canonical)
{
    if (v) {
        try {
            JSONEncoder encoder;
            encoder.setSharedKeys(sk);
            encoder.setJSON5(json5);
            encoder.setCanonical(canonical);
            encoder.writeValue(v);
            return toSliceResult(encoder.extractOutput());
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLValue_ToJSON(FLValue v)      {return FLValue_ToJSONX(v, nullptr, false, false);}
FLSliceResult FLValue_ToJSON5(FLValue v)     {return FLValue_ToJSONX(v, nullptr, true,  false);}


FLSliceResult FLData_ConvertJSON(FLSlice json, FLError *outError) {
    FLEncoderImpl e(kFLEncodeFleece, json.size);
    FLEncoder_ConvertJSON(&e, json);
    return FLEncoder_Finish(&e, outError);
}


FLSliceResult FLJSON5_ToJSON(FLSlice json5, FLError *error) {
    try {
        std::string json = ConvertJSON5((std::string((char*)json5.buf, json5.size)));
        return toSliceResult(alloc_slice(json));
    } catchError(nullptr)
    return {};
}


FLSliceResult FLData_Dump(FLSlice data) {
    try {
        return toSliceResult(alloc_slice(Value::dump(data)));
    } catchError(nullptr)
    return {nullptr, 0};
}


#pragma mark - ARRAYS:


uint32_t FLArray_Count(FLArray a)                    {return a ? a->count() : 0;}
bool FLArray_IsEmpty(FLArray a)                      {return a ? a->empty() : true;}
FLValue FLArray_Get(FLArray a, uint32_t index)       {return a ? a->get(index) : nullptr;}

void FLArrayIterator_Begin(FLArray a, FLArrayIterator* i) {
    static_assert(sizeof(FLArrayIterator) >= sizeof(Array::iterator),"FLArrayIterator is too small");
    new (i) Array::iterator(a);
    // Note: this is safe even if a is null.
}

uint32_t FLArrayIterator_GetCount(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->count();
}

FLValue FLArrayIterator_GetValue(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->value();
}

FLValue FLArrayIterator_GetValueAt(const FLArrayIterator *i, uint32_t offset) {
    return (*(Array::iterator*)i)[offset];
}

bool FLArrayIterator_Next(FLArrayIterator* i) {
    try {
        auto& iter = *(Array::iterator*)i;
        ++iter;                 // throws if iterating past end
        return (bool)iter;
    } catchError(nullptr)
    return false;
}


#pragma mark - DICTIONARIES:


uint32_t FLDict_Count(FLDict d)                          {return d ? d->count() : 0;}
bool FLDict_IsEmpty(FLDict d)                            {return d ? d->empty() : true;}
FLValue FLDict_Get(FLDict d, FLSlice keyString)          {return d ? d->get(keyString) : nullptr;}
FLValue FLDict_GetUnsorted(FLDict d, FLSlice keyString)  {return d ? d->get_unsorted(keyString) : nullptr;}

FLValue FLDict_GetSharedKey(FLDict d, FLSlice keyString, FLSharedKeys sk) {
    return d ? d->get(keyString, sk) : nullptr;
}

FLSlice FLSharedKey_GetKeyString(FLSharedKeys sk, int keyCode, FLError* outError)
{
    slice key;
    try {
        key = sk->decode((keyCode));
        if(!key && outError != nullptr) {
            *outError = kFLNotFound;
        }
    } catchError(outError)
    
    return key;
}

void FLDictIterator_Begin(FLDict d, FLDictIterator* i) {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d);
    // Note: this is safe even if d is null.
}

void FLDictIterator_BeginShared(FLDict d, FLDictIterator* i, FLSharedKeys sk) {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d, sk);
    // Note: this is safe even if d is null.
}

FLValue FLDictIterator_GetKey(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->key();
}

FLString FLDictIterator_GetKeyString(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->keyString();
}

FLValue FLDictIterator_GetValue(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->value();
}

uint32_t FLDictIterator_GetCount(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->count();
}

bool FLDictIterator_Next(FLDictIterator* i) {
    try {
        auto& iter = *(Dict::iterator*)i;
        ++iter;                 // throws if iterating past end
        return (bool)iter;
    } catchError(nullptr)
    return false;
}


FLDictKey FLDictKey_Init(FLSlice string, bool cachePointers) {
    FLDictKey key;
    static_assert(sizeof(FLDictKey) >= sizeof(Dict::key), "FLDictKey is too small");
    new (&key) Dict::key(string, nullptr, cachePointers);
    return key;
}

FLDictKey FLDictKey_InitWithSharedKeys(FLSlice string, FLSharedKeys sharedKeys) {
    FLDictKey key;
    static_assert(sizeof(FLDictKey) >= sizeof(Dict::key), "FLDictKey is too small");
    new (&key) Dict::key(string, (SharedKeys*)sharedKeys, false);
    return key;
}

FLSlice FLDictKey_GetString(const FLDictKey *key) {
    auto realKey = (const Dict::key*)key;
    return realKey->string();
}

FLValue FLDict_GetWithKey(FLDict d, FLDictKey *k) {
    if (!d)
        return nullptr;
    auto key = *(Dict::key*)k;
    return d->get(key);
}

size_t FLDict_GetWithKeys(FLDict d, FLDictKey keys[], FLValue values[], size_t count) {
    return d->get((Dict::key*)keys, values, count);
}


#pragma mark - DEEP ITERATOR:


FLDeepIterator FLDeepIterator_New(FLValue v, FLSharedKeys sk)   {return new DeepIterator(v, sk);}
void FLDeepIterator_Free(FLDeepIterator i)                      {delete i;}
FLValue FLDeepIterator_GetValue(FLDeepIterator i)               {return i->value();}
FLSlice FLDeepIterator_GetKey(FLDeepIterator i)                 {return i->keyString();}
uint32_t FLDeepIterator_GetIndex(FLDeepIterator i)              {return i->index();}
size_t FLDeepIterator_GetDepth(FLDeepIterator i)                {return i->path().size();}
void FLDeepIterator_SkipChildren(FLDeepIterator i)              {i->skipChildren();}

bool FLDeepIterator_Next(FLDeepIterator i) {
    i->next();
    return i->value() != nullptr;
}

void FLDeepIterator_GetPath(FLDeepIterator i, FLPathComponent* *outPath, size_t *outDepth) {
    static_assert(sizeof(FLPathComponent) == sizeof(DeepIterator::PathComponent),
                  "FLPathComponent does not match PathComponent");
    auto &path = i->path();
    *outPath = (FLPathComponent*) path.data();
    *outDepth = path.size();
}

FLSliceResult FLDeepIterator_GetJSONPointer(FLDeepIterator i) {
    return toSliceResult(alloc_slice(i->jsonPointer()));
}


#pragma mark - KEY-PATHS:


FLKeyPath FLKeyPath_New(FLSlice specifier, FLSharedKeys sharedKeys, FLError *outError) {
    try {
        return new Path((std::string)(slice)specifier, sharedKeys);
    } catchError(outError)
    return nullptr;
}

void FLKeyPath_Free(FLKeyPath path) {
    delete path;
}

FLValue FLKeyPath_Eval(FLKeyPath path, FLValue root) {
    return path->eval(root);
}

FLValue FLKeyPath_EvalOnce(FLSlice specifier, FLSharedKeys sharedKeys, FLValue root,
                           FLError *outError)
{
    try {
        return Path::eval((std::string)(slice)specifier, sharedKeys, root);
    } catchError(outError)
    return nullptr;
}


#pragma mark - ENCODER:


FLEncoder FLEncoder_New(void) {
    return FLEncoder_NewWithOptions(kFLEncodeFleece, 0, true, true);
}

FLEncoder FLEncoder_NewWithOptions(FLEncoderFormat format,
                                   size_t reserveSize, bool uniqueStrings, bool sortKeys)
{
    return new FLEncoderImpl(format, reserveSize, uniqueStrings, sortKeys);
}

void FLEncoder_Reset(FLEncoder e) {
    e->reset();
}

void FLEncoder_Free(FLEncoder e)                         {
    delete e;
}

void FLEncoder_SetSharedKeys(FLEncoder e, FLSharedKeys sk) {
    ENCODER_DO(e, setSharedKeys(sk));
}

void FLEncoder_MakeDelta(FLEncoder e, FLSlice base, bool reuseStrings) {
    if (e->isFleece()) {
        e->fleeceEncoder->setBase(base);
        if(reuseStrings)
            e->fleeceEncoder->reuseBaseStrings();
    }
}

size_t FLEncoder_BytesWritten(FLEncoder e) {
    return ENCODER_DO(e, bytesWritten());
}

bool FLEncoder_WriteNull(FLEncoder e)                    {ENCODER_TRY(e, writeNull());}
bool FLEncoder_WriteBool(FLEncoder e, bool b)            {ENCODER_TRY(e, writeBool(b));}
bool FLEncoder_WriteInt(FLEncoder e, int64_t i)          {ENCODER_TRY(e, writeInt(i));}
bool FLEncoder_WriteUInt(FLEncoder e, uint64_t u)        {ENCODER_TRY(e, writeUInt(u));}
bool FLEncoder_WriteFloat(FLEncoder e, float f)          {ENCODER_TRY(e, writeFloat(f));}
bool FLEncoder_WriteDouble(FLEncoder e, double d)        {ENCODER_TRY(e, writeDouble(d));}
bool FLEncoder_WriteString(FLEncoder e, FLSlice s)       {ENCODER_TRY(e, writeString(s));}
bool FLEncoder_WriteData(FLEncoder e, FLSlice d)         {ENCODER_TRY(e, writeData(d));}
bool FLEncoder_WriteRaw(FLEncoder e, FLSlice r)          {ENCODER_TRY(e, writeRaw(r));}

bool FLEncoder_BeginArray(FLEncoder e, size_t reserve)   {ENCODER_TRY(e, beginArray(reserve));}
bool FLEncoder_EndArray(FLEncoder e)                     {ENCODER_TRY(e, endArray());}
bool FLEncoder_BeginDict(FLEncoder e, size_t reserve)    {ENCODER_TRY(e, beginDictionary(reserve));}
bool FLEncoder_WriteKey(FLEncoder e, FLSlice s)          {ENCODER_TRY(e, writeKey(s));}
bool FLEncoder_EndDict(FLEncoder e)                      {ENCODER_TRY(e, endDictionary());}

bool FLEncoder_WriteValueWithSharedKeys(FLEncoder e, FLValue v, FLSharedKeys sk)
                                                         {ENCODER_TRY(e, writeValue(v, sk));}
bool FLEncoder_WriteValue(FLEncoder e, FLValue v) {
    return FLEncoder_WriteValueWithSharedKeys(e, v, nullptr);
}



bool FLEncoder_ConvertJSON(FLEncoder e, FLSlice json) {
    if (!e->hasError()) {
        try {
            if (e->isFleece()) {
                JSONConverter *jc = e->jsonConverter.get();
                if (jc) {
                    jc->reset();
                } else {
                    jc = new JSONConverter(*e->fleeceEncoder);
                    e->jsonConverter.reset(jc);
                }
                if (jc->encodeJSON(json)) {                   // encodeJSON can throw
                    return true;
                } else {
                    e->errorCode = (FLError)jc->errorCode();
                    e->errorMessage = jc->errorMessage();
                }
            } else {
                e->jsonEncoder->writeJSON(json);
            }
        } catch (const std::exception &x) {
            e->recordException(x);
        }
    }
    return false;
}

FLError FLEncoder_GetError(FLEncoder e) {
    return (FLError)e->errorCode;
}

const char* FLEncoder_GetErrorMessage(FLEncoder e) {
    return e->hasError() ? e->errorMessage.c_str() : nullptr;
}

void FLEncoder_SetExtraInfo(FLEncoder e, void *info) {
    e->extraInfo = info;
}

void* FLEncoder_GetExtraInfo(FLEncoder e) {
    return e->extraInfo;
}

FLSliceResult FLEncoder_Finish(FLEncoder e, FLError *outError) {
    if (!e->hasError()) {
        try {
            return toSliceResult(ENCODER_DO(e, extractOutput()));       // extractOutput can throw
        } catch (const std::exception &x) {
            e->recordException(x);
        }
    }
    // Failure:
    if (outError)
        *outError = e->errorCode;
    return {nullptr, 0};
}
