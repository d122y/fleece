//
//  DeepIterator.cc
//  Fleece
//
//  Created by Jens Alfke on 4/17/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "DeepIterator.hh"
#include "SharedKeys.hh"
#include <sstream>

namespace fleece {

    DeepIterator::DeepIterator(const Value *root, SharedKeys *sk)
    :_sk(sk)
    ,_value(root)
    ,_skipChildren(false)
    { }

    void DeepIterator::next() {
        if (!_value)
            return;

        if (_skipChildren)
            _skipChildren = false;
        else if (_path.empty())
            iterateContainer(_value);
        else
            queueChildren();

        if (!_path.empty())
            _path.pop_back();

        do {
            if (_arrayIt) {
                // Next array item:
                _value = (*_arrayIt).value();
                if (_value) {
                    _path.push_back({nullslice, _arrayIndex++});
                    ++(*_arrayIt);
                } else {
                    _arrayIt.reset();
                }
            } else if (_dictIt) {
                // Next dict item:
                _value = (*_dictIt).value();
                if (_value) {
                    _path.push_back({(*_dictIt).keyString(), 0});
                    ++(*_dictIt);
                } else {
                    _dictIt.reset();
                }
            } else {
                // End of array/dict, so start another one:
                _value = nullptr;
                while (_stack.front().second == nullptr) {
                    // end of a level of hierarchy; pop the path, or stop if it's empty:
                    if (_path.empty())
                        return; // end of iteration
                    _path.pop_back();
                    _stack.pop_front();
                }

                // Pop the next container and its key from the stack:
                auto container = _stack.front().second;
                _path.push_back(_stack.front().first);
                _stack.pop_front();
                iterateContainer(container);
            }
        } while (!_value);
    }

    bool DeepIterator::iterateContainer(const Value *container) {
        _stack.push_front({{nullslice, 0}, nullptr});   // Push en end-of-level marker first
        auto type = container->type();
        if (type == kArray) {
            _arrayIt.reset( new Array::iterator(container->asArray()) );
            _arrayIndex = 0;
            return true;
        } else if (type == kDict) {
            _dictIt.reset( new Dict::iterator(container->asDict(), _sk) );
            return true;
        } else {
            return false;
        }
    }

    void DeepIterator::queueChildren() {
        auto type = _value->type();
        if (type == kDict || type == kArray)
            _stack.push_front({_path.back(), _value});
    }


    std::string DeepIterator::jsonPointer() {
        if (_path.empty())
            return "/";
        std::stringstream s;
        for (auto &component : _path) {
            s << '/';
            if (component.key) {
                // Keys need to be escaped per https://tools.ietf.org/html/rfc6901#section-3 :
                if (component.key.findAnyByteOf("/~"_sl)) {
                    auto end = (const char*)component.key.end();
                    for (auto c = (const char*)component.key.buf; c != end; ++c) {
                        if (*c == '/')
                            s << "~1";
                        else if (*c == '~')
                            s << "~0";
                        else
                            s << *c;
                    }
                } else {
                    s.write((char*)component.key.buf, component.key.size);
                }
            } else {
                s << component.index;
            }
        }
        return s.str();
    }

}
