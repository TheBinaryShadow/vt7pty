// Copyright (c) 2011-2015 Ryan Prichard
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

#include "InputMap.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DebugShowInput.h"
#include "SimplePool.h"
#include "../shared/DebugClient.h"
#include "../shared/ControlCharacters.h"
#include "../shared/Assert.h"
#include "../shared/StringFormatting.h"

namespace {

static const char *getVirtualKeyString(int virtualKey)
{
    switch (virtualKey) {
#define VT7PTY_GVKS_KEY(x) case VK_##x: return #x;
        VT7PTY_GVKS_KEY(RBUTTON)    VT7PTY_GVKS_KEY(F9)
        VT7PTY_GVKS_KEY(CANCEL)     VT7PTY_GVKS_KEY(F10)
        VT7PTY_GVKS_KEY(MBUTTON)    VT7PTY_GVKS_KEY(F11)
        VT7PTY_GVKS_KEY(XBUTTON1)   VT7PTY_GVKS_KEY(F12)
        VT7PTY_GVKS_KEY(XBUTTON2)   VT7PTY_GVKS_KEY(F13)
        VT7PTY_GVKS_KEY(BACK)       VT7PTY_GVKS_KEY(F14)
        VT7PTY_GVKS_KEY(TAB)        VT7PTY_GVKS_KEY(F15)
        VT7PTY_GVKS_KEY(CLEAR)      VT7PTY_GVKS_KEY(F16)
        VT7PTY_GVKS_KEY(RETURN)     VT7PTY_GVKS_KEY(F17)
        VT7PTY_GVKS_KEY(SHIFT)      VT7PTY_GVKS_KEY(F18)
        VT7PTY_GVKS_KEY(CONTROL)    VT7PTY_GVKS_KEY(F19)
        VT7PTY_GVKS_KEY(MENU)       VT7PTY_GVKS_KEY(F20)
        VT7PTY_GVKS_KEY(PAUSE)      VT7PTY_GVKS_KEY(F21)
        VT7PTY_GVKS_KEY(CAPITAL)    VT7PTY_GVKS_KEY(F22)
        VT7PTY_GVKS_KEY(HANGUL)     VT7PTY_GVKS_KEY(F23)
        VT7PTY_GVKS_KEY(JUNJA)      VT7PTY_GVKS_KEY(F24)
        VT7PTY_GVKS_KEY(FINAL)      VT7PTY_GVKS_KEY(NUMLOCK)
        VT7PTY_GVKS_KEY(KANJI)      VT7PTY_GVKS_KEY(SCROLL)
        VT7PTY_GVKS_KEY(ESCAPE)     VT7PTY_GVKS_KEY(LSHIFT)
        VT7PTY_GVKS_KEY(CONVERT)    VT7PTY_GVKS_KEY(RSHIFT)
        VT7PTY_GVKS_KEY(NONCONVERT) VT7PTY_GVKS_KEY(LCONTROL)
        VT7PTY_GVKS_KEY(ACCEPT)     VT7PTY_GVKS_KEY(RCONTROL)
        VT7PTY_GVKS_KEY(MODECHANGE) VT7PTY_GVKS_KEY(LMENU)
        VT7PTY_GVKS_KEY(SPACE)      VT7PTY_GVKS_KEY(RMENU)
        VT7PTY_GVKS_KEY(PRIOR)      VT7PTY_GVKS_KEY(BROWSER_BACK)
        VT7PTY_GVKS_KEY(NEXT)       VT7PTY_GVKS_KEY(BROWSER_FORWARD)
        VT7PTY_GVKS_KEY(END)        VT7PTY_GVKS_KEY(BROWSER_REFRESH)
        VT7PTY_GVKS_KEY(HOME)       VT7PTY_GVKS_KEY(BROWSER_STOP)
        VT7PTY_GVKS_KEY(LEFT)       VT7PTY_GVKS_KEY(BROWSER_SEARCH)
        VT7PTY_GVKS_KEY(UP)         VT7PTY_GVKS_KEY(BROWSER_FAVORITES)
        VT7PTY_GVKS_KEY(RIGHT)      VT7PTY_GVKS_KEY(BROWSER_HOME)
        VT7PTY_GVKS_KEY(DOWN)       VT7PTY_GVKS_KEY(VOLUME_MUTE)
        VT7PTY_GVKS_KEY(SELECT)     VT7PTY_GVKS_KEY(VOLUME_DOWN)
        VT7PTY_GVKS_KEY(PRINT)      VT7PTY_GVKS_KEY(VOLUME_UP)
        VT7PTY_GVKS_KEY(EXECUTE)    VT7PTY_GVKS_KEY(MEDIA_NEXT_TRACK)
        VT7PTY_GVKS_KEY(SNAPSHOT)   VT7PTY_GVKS_KEY(MEDIA_PREV_TRACK)
        VT7PTY_GVKS_KEY(INSERT)     VT7PTY_GVKS_KEY(MEDIA_STOP)
        VT7PTY_GVKS_KEY(DELETE)     VT7PTY_GVKS_KEY(MEDIA_PLAY_PAUSE)
        VT7PTY_GVKS_KEY(HELP)       VT7PTY_GVKS_KEY(LAUNCH_MAIL)
        VT7PTY_GVKS_KEY(LWIN)       VT7PTY_GVKS_KEY(LAUNCH_MEDIA_SELECT)
        VT7PTY_GVKS_KEY(RWIN)       VT7PTY_GVKS_KEY(LAUNCH_APP1)
        VT7PTY_GVKS_KEY(APPS)       VT7PTY_GVKS_KEY(LAUNCH_APP2)
        VT7PTY_GVKS_KEY(SLEEP)      VT7PTY_GVKS_KEY(OEM_1)
        VT7PTY_GVKS_KEY(NUMPAD0)    VT7PTY_GVKS_KEY(OEM_PLUS)
        VT7PTY_GVKS_KEY(NUMPAD1)    VT7PTY_GVKS_KEY(OEM_COMMA)
        VT7PTY_GVKS_KEY(NUMPAD2)    VT7PTY_GVKS_KEY(OEM_MINUS)
        VT7PTY_GVKS_KEY(NUMPAD3)    VT7PTY_GVKS_KEY(OEM_PERIOD)
        VT7PTY_GVKS_KEY(NUMPAD4)    VT7PTY_GVKS_KEY(OEM_2)
        VT7PTY_GVKS_KEY(NUMPAD5)    VT7PTY_GVKS_KEY(OEM_3)
        VT7PTY_GVKS_KEY(NUMPAD6)    VT7PTY_GVKS_KEY(OEM_4)
        VT7PTY_GVKS_KEY(NUMPAD7)    VT7PTY_GVKS_KEY(OEM_5)
        VT7PTY_GVKS_KEY(NUMPAD8)    VT7PTY_GVKS_KEY(OEM_6)
        VT7PTY_GVKS_KEY(NUMPAD9)    VT7PTY_GVKS_KEY(OEM_7)
        VT7PTY_GVKS_KEY(MULTIPLY)   VT7PTY_GVKS_KEY(OEM_8)
        VT7PTY_GVKS_KEY(ADD)        VT7PTY_GVKS_KEY(OEM_102)
        VT7PTY_GVKS_KEY(SEPARATOR)  VT7PTY_GVKS_KEY(PROCESSKEY)
        VT7PTY_GVKS_KEY(SUBTRACT)   VT7PTY_GVKS_KEY(PACKET)
        VT7PTY_GVKS_KEY(DECIMAL)    VT7PTY_GVKS_KEY(ATTN)
        VT7PTY_GVKS_KEY(DIVIDE)     VT7PTY_GVKS_KEY(CRSEL)
        VT7PTY_GVKS_KEY(F1)         VT7PTY_GVKS_KEY(EXSEL)
        VT7PTY_GVKS_KEY(F2)         VT7PTY_GVKS_KEY(EREOF)
        VT7PTY_GVKS_KEY(F3)         VT7PTY_GVKS_KEY(PLAY)
        VT7PTY_GVKS_KEY(F4)         VT7PTY_GVKS_KEY(ZOOM)
        VT7PTY_GVKS_KEY(F5)         VT7PTY_GVKS_KEY(NONAME)
        VT7PTY_GVKS_KEY(F6)         VT7PTY_GVKS_KEY(PA1)
        VT7PTY_GVKS_KEY(F7)         VT7PTY_GVKS_KEY(OEM_CLEAR)
        VT7PTY_GVKS_KEY(F8)
#undef VT7PTY_GVKS_KEY
        default:                        return NULL;
    }
}

} // anonymous namespace

std::string InputMap::Key::toString() const {
    std::string ret;
    ret += controlKeyStatePrefix(keyState);
    char buf[256];
    const char *vkString = getVirtualKeyString(virtualKey);
    if (vkString != NULL) {
        ret += vkString;
    } else if ((virtualKey >= 'A' && virtualKey <= 'Z') ||
               (virtualKey >= '0' && virtualKey <= '9')) {
        ret += static_cast<char>(virtualKey);
    } else {
        formatString(buf, "%#x", virtualKey);
        ret += buf;
    }
    if (unicodeChar >= 32 && unicodeChar <= 126) {
        formatString(buf, " ch='%c'",
                        static_cast<char>(unicodeChar));
    } else {
        formatString(buf, " ch=%#x",
                        static_cast<unsigned int>(unicodeChar));
    }
    ret += buf;
    return ret;
}

void InputMap::set(const char *encoding, int encodingLen, const Key &key) {
    ASSERT(encodingLen > 0);
    setHelper(m_root, encoding, encodingLen, key);
}

void InputMap::setHelper(Node &node, const char *encoding, int encodingLen, const Key &key) {
    if (encodingLen == 0) {
        node.key = key;
    } else {
        setHelper(getOrCreateChild(node, encoding[0]), encoding + 1, encodingLen - 1, key);
    }
}

InputMap::Node &InputMap::getOrCreateChild(Node &node, unsigned char ch) {
    Node *ret = getChild(node, ch);
    if (ret != NULL) {
        return *ret;
    }
    if (node.childCount < Node::kTinyCount) {
        // Maintain sorted order for the sake of the InputMap dumping.
        int insertIndex = node.childCount;
        for (int i = 0; i < node.childCount; ++i) {
            if (ch < node.u.tiny.values[i]) {
                insertIndex = i;
                break;
            }
        }
        for (int j = node.childCount; j > insertIndex; --j) {
            node.u.tiny.values[j] = node.u.tiny.values[j - 1];
            node.u.tiny.children[j] = node.u.tiny.children[j - 1];
        }
        node.u.tiny.values[insertIndex] = ch;
        node.u.tiny.children[insertIndex] = ret = m_nodePool.alloc();
        ++node.childCount;
        return *ret;
    }
    if (node.childCount == Node::kTinyCount) {
        Branch *branch = m_branchPool.alloc();
        for (int i = 0; i < node.childCount; ++i) {
            branch->children[node.u.tiny.values[i]] = node.u.tiny.children[i];
        }
        node.u.branch = branch;
    }
    node.u.branch->children[ch] = ret = m_nodePool.alloc();
    ++node.childCount;
    return *ret;
}

// Find the longest matching key and node.
int InputMap::lookupKey(const char *input, int inputSize,
                        Key &keyOut, bool &incompleteOut) const {
    keyOut = kKeyZero;
    incompleteOut = false;

    const Node *node = &m_root;
    InputMap::Key longestMatch = kKeyZero;
    int longestMatchLen = 0;

    for (int i = 0; i < inputSize; ++i) {
        unsigned char ch = input[i];
        node = getChild(*node, ch);
        if (node == NULL) {
            keyOut = longestMatch;
            return longestMatchLen;
        } else if (node->hasKey()) {
            longestMatchLen = i + 1;
            longestMatch = node->key;
        }
    }
    keyOut = longestMatch;
    incompleteOut = node->childCount > 0;
    return longestMatchLen;
}

void InputMap::dumpInputMap() const {
    std::string encoding;
    dumpInputMapHelper(m_root, encoding);
}

void InputMap::dumpInputMapHelper(
        const Node &node, std::string &encoding) const {
    if (node.hasKey()) {
        trace("%s -> %s",
            encoding.c_str(),
            node.key.toString().c_str());
    }
    for (int i = 0; i < 256; ++i) {
        const Node *child = getChild(node, i);
        if (child != NULL) {
            size_t oldSize = encoding.size();
            if (!encoding.empty()) {
                encoding.push_back(' ');
            }
            char ctrlChar = decodeControlCharacter(i);
            if (ctrlChar != '\0') {
                encoding.push_back('^');
                encoding.push_back(static_cast<char>(ctrlChar));
            } else if (i == ' ') {
                encoding.append("' '");
            } else {
                encoding.push_back(static_cast<char>(i));
            }
            dumpInputMapHelper(*child, encoding);
            encoding.resize(oldSize);
        }
    }
}
