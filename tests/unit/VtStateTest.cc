// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "../fidelity/VtState.h"
#include "../fidelity/WinTest.h"
#include <cstdio>
#include <functional>
using fidelity::VtState;
static void require(bool value, const char *label) { if (!value) throw std::runtime_error(label); }
static void rejected(const std::function<void()> &test) {
    try { test(); } catch (const std::runtime_error &) { return; }
    throw std::runtime_error("oracle accepted malformed/unsupported input");
}
int main() {
    try {
        const std::string stream = "\x1b[2J\x1b[2;3H\x1b[31;44mAB\x1b[0m\x1b[4;5H!";
        for (size_t split = 0; split <= stream.size(); ++split) {
            VtState state(12,6); state.feed(std::string_view(stream).substr(0,split));
            state.feed(std::string_view(stream).substr(split)); state.finish();
            require(state.at(2,1).text == U"A" && state.at(3,1).text == U"B", "position oracle");
            require(state.at(2,1).attributes == 0x14, "attribute oracle");
            require(state.x() == 5 && state.y() == 3, "cursor oracle");
        }
        VtState erase(8,3); erase.feed("ABCDEFG\x1b[1;4H\x1b[0K");
        require(erase.row(0) == U"ABC     ", "erase line oracle");
        erase.feed("\x1b[2;1HXYZ\x1b[1J");
        require(erase.row(0) == U"        " && erase.at(0,1).text == U" ", "erase display oracle");
        VtState wrap(4,2); wrap.feed("abcdEFGH!");
        require(wrap.row(0) == U"EFGH" && wrap.row(1) == U"!   ", "wrap/scroll oracle");
        VtState unicode(20,3);
        const std::string text = "\xc4\x87" "e\xcc\x81" "\xe4\xb8\xad" "\xf0\x9f\x99\x82";
        for (char c : text) unicode.feed(std::string_view(&c,1));
        unicode.finish();
        require(unicode.at(0,0).text == U"\u0107" && unicode.at(1,0).text == U"e\u0301", "Unicode oracle");
        require(unicode.at(2,0).text == U"\u4e2d" && unicode.at(3,0).continuation &&
                unicode.at(4,0).text == U"\U0001f642" && unicode.x() == 6, "wide corpus oracle");
        VtState replacement(8,2); replacement.feed("\xe4\xb8\xad\x1b[2GX");
        require(replacement.at(0,0).text == U" " && replacement.at(1,0).text == U"X" &&
            !replacement.at(1,0).continuation,"wide overwrite oracle");
        replacement.feed("\x1b[4G\xe4\xb8\xad\x1b[5G\x1b[0K");
        require(replacement.at(3,0).text == U" " && !replacement.at(4,0).continuation,"wide erase oracle");
        require(fidelity::quote(L"C:\\a path\\") == L"\"C:\\a path\\\\\"" &&
            fidelity::quote(L"a\"b") == L"\"a\\\"b\"","Windows argument quoting oracle");
        auto packetStorage=std::make_unique<fidelity::Packet>();
        auto &packet=*packetStorage; packet.columns=80; packet.rows=25; packet.bufferColumns=80; packet.bufferRows=25;
        fidelity::validate(packet);
        rejected([&] { packet.cursorX=80; fidelity::validate(packet); });
        require(fidelity::json("\xc4\x87") == "\"\xc4\x87\"","UTF-8 metadata JSON");
        VtState resized(8,3); resized.feed("ABC\x1b[3;8HX"); resized.resize(4,2);
        require(resized.row(0) == U"ABC " && resized.x() == 3 && resized.y() == 1,"resize clipping oracle");
        rejected([&] { resized.at(4,0); });
        VtState attrs(8,2); attrs.feed("\x1b[1;7;4mX\x1b[0mY");
        require(attrs.at(0,0).attributes == 0x80f0 && attrs.at(1,0).attributes == 7, "SGR reset/inverse oracle");
        attrs.feed("\x1b[?25l\x1b[6n");
        require(!attrs.visible() && attrs.takeReplies() == std::vector<std::string>{"\x1b[1;3R"}, "cursor/DSR oracle");
        attrs.feed("\x1b]0;title\x07\x1b]2;title\x1b\\");
        attrs.finish();
        rejected([] { VtState s(8,2); s.feed("\x1b[38;2;255;0;0m"); });
        rejected([] { VtState s(8,2); s.feed("\x1b[999999999G"); });
        rejected([] { VtState s(8,2); s.feed("\x1b[?1049h"); });
        rejected([] { VtState s(8,2); s.feed("\xc0\x80"); });
        rejected([] { VtState s(8,2); s.feed("\xed\xa0\x80"); });
        rejected([] { VtState s(8,2); s.feed("\xf4\x90\x80\x80"); });
        rejected([] { VtState s(8,2); s.feed("\xe4"); s.finish(); });
        rejected([] { VtState s(8,2); s.feed("\x1b["); s.finish(); });
        rejected([] { VtState s(8,2); s.feed("\x1b]0;" + std::string(1025,'x')); });
        // These intentional mutations prove separate state dimensions detect faults.
        VtState correct(8,2); correct.feed("\x1b[31mOK");
        VtState wrongCell(8,2); wrongCell.feed("\x1b[31mNO");
        VtState wrongAttr(8,2); wrongAttr.feed("OK");
        VtState wrongCursor(8,2); wrongCursor.feed("\x1b[31mOK\x1b[1G");
        require(correct.at(0,0) != wrongCell.at(0,0), "cell negative control");
        require(correct.at(0,0).attributes != wrongAttr.at(0,0).attributes, "attribute negative control");
        require(correct.x() != wrongCursor.x(), "cursor negative control");
        puts("VT7Pty VT state oracle tests passed.");
        return 0;
    } catch (const std::exception &e) { fprintf(stderr,"%s\n",e.what()); return 1; }
}
