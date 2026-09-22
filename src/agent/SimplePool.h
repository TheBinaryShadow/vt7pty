// Copyright (c) 2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef SIMPLE_POOL_H
#define SIMPLE_POOL_H

#include <cstddef>
#include <memory>
#include <vector>

template <typename T, size_t chunkSize>
class SimplePool {
public:
    ~SimplePool();
    T *alloc();
    void clear();
private:
    struct Chunk {
        size_t count;
        std::unique_ptr<T[]> data;
    };
    std::vector<Chunk> m_chunks;
};

template <typename T, size_t chunkSize>
SimplePool<T, chunkSize>::~SimplePool() {
    clear();
}

template <typename T, size_t chunkSize>
void SimplePool<T, chunkSize>::clear() {
    m_chunks.clear();
}

template <typename T, size_t chunkSize>
T *SimplePool<T, chunkSize>::alloc() {
    if (m_chunks.empty() || m_chunks.back().count == chunkSize) {
        Chunk newChunk = { 0, std::make_unique<T[]>(chunkSize) };
        m_chunks.push_back(std::move(newChunk));
    }
    Chunk &chunk = m_chunks.back();
    return &chunk.data[chunk.count++];
}

#endif // SIMPLE_POOL_H
