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

#ifndef SMALLRECT_H
#define SMALLRECT_H

#include <windows.h>

#include <algorithm>
#include <format>
#include <string>

#include "../shared/Narrow.h"
#include "Coord.h"

struct SmallRect : SMALL_RECT
{
    SmallRect()
    {
        Left = Right = Top = Bottom = 0;
    }

    SmallRect(SHORT x, SHORT y, SHORT width, SHORT height)
    {
        Left = x;
        Top = y;
        Right = vt7pty::internal::checkedNarrow<SHORT>(x + width - 1);
        Bottom = vt7pty::internal::checkedNarrow<SHORT>(y + height - 1);
    }

    SmallRect(const COORD &topLeft, const COORD &size)
    {
        Left = topLeft.X;
        Top = topLeft.Y;
        Right = vt7pty::internal::checkedNarrow<SHORT>(Left + size.X - 1);
        Bottom = vt7pty::internal::checkedNarrow<SHORT>(Top + size.Y - 1);
    }

    SmallRect(const SMALL_RECT &other) : SMALL_RECT(other) {}
    SmallRect(const SmallRect &) = default;
    SmallRect &operator=(const SmallRect &) = default;

    bool contains(const SmallRect &other) const
    {
        return other.Left >= Left &&
               other.Right <= Right &&
               other.Top >= Top &&
               other.Bottom <= Bottom;
    }

    bool contains(const Coord &other) const
    {
        return other.X >= Left &&
               other.X <= Right &&
               other.Y >= Top &&
               other.Y <= Bottom;
    }

    SmallRect intersected(const SmallRect &other) const
    {
        int x1 = std::max(Left, other.Left);
        int x2 = std::min(Right, other.Right);
        int y1 = std::max(Top, other.Top);
        int y2 = std::min(Bottom, other.Bottom);
        return SmallRect(
            vt7pty::internal::checkedNarrow<SHORT>(x1),
            vt7pty::internal::checkedNarrow<SHORT>(y1),
            vt7pty::internal::checkedNarrow<SHORT>(std::max(0, x2 - x1 + 1)),
            vt7pty::internal::checkedNarrow<SHORT>(std::max(0, y2 - y1 + 1)));
    }

    SmallRect ensureLineIncluded(SHORT line) const
    {
        const SHORT h = height();
        if (line < Top) {
            return SmallRect(Left, line, width(), h);
        } else if (line > Bottom) {
            return SmallRect(Left,
                vt7pty::internal::checkedNarrow<SHORT>(line - h + 1),
                width(), h);
        } else {
            return *this;
        }
    }

    SHORT top() const               { return Top;                       }
    SHORT left() const              { return Left;                      }
    SHORT width() const             { return vt7pty::internal::checkedNarrow<SHORT>(Right - Left + 1); }
    SHORT height() const            { return vt7pty::internal::checkedNarrow<SHORT>(Bottom - Top + 1); }
    void setTop(SHORT top)          { Top = top;                        }
    void setLeft(SHORT left)        { Left = left;                      }
    void setWidth(SHORT width)      { Right = vt7pty::internal::checkedNarrow<SHORT>(Left + width - 1); }
    void setHeight(SHORT height)    { Bottom = vt7pty::internal::checkedNarrow<SHORT>(Top + height - 1); }
    Coord size() const              { return Coord(width(), height());  }

    bool operator==(const SmallRect &other) const
    {
        return Left == other.Left &&
               Right == other.Right &&
               Top == other.Top &&
               Bottom == other.Bottom;
    }

    bool operator!=(const SmallRect &other) const
    {
        return !(*this == other);
    }

    std::string toString() const
    {
        return std::format("(x={},y={},w={},h={})",
            Left, Top, width(), height());
    }
};

#endif // SMALLRECT_H
