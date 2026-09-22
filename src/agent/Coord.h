// Copyright (c) 2011-2012 Ryan Prichard
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

#ifndef COORD_H
#define COORD_H

#include <windows.h>

#include <format>
#include <string>

#include "../shared/Narrow.h"

struct Coord : COORD {
    Coord()
    {
        X = 0;
        Y = 0;
    }

    Coord(SHORT x, SHORT y)
    {
        X = x;
        Y = y;
    }

    Coord(COORD other) : Coord(other.X, other.Y) {}
    Coord(const Coord &) = default;
    Coord &operator=(const Coord &) = default;

    bool operator==(const Coord &other) const
    {
        return X == other.X && Y == other.Y;
    }

    bool operator!=(const Coord &other) const
    {
        return !(*this == other);
    }

    Coord operator+(const Coord &other) const
    {
        return Coord(
            vt7pty::internal::checkedNarrow<SHORT>(X + other.X),
            vt7pty::internal::checkedNarrow<SHORT>(Y + other.Y));
    }

    bool isEmpty() const
    {
        return X <= 0 || Y <= 0;
    }

    std::string toString() const
    {
        return std::format("({},{})", X, Y);
    }
};

#endif // COORD_H
