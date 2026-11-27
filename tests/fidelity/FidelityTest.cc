// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#include "WinTest.h"
#include "VtState.h"
#include "OutputCorpus.h"
#include "ScreenCases.h"
#include "LifecycleCases.h"
#include "../../src/include/vt7pty.h"
#define VT7PTY_LIFECYCLE_TRACE_READER
#include "../../src/shared/LifecycleResizeTrace.h"
#undef VT7PTY_LIFECYCLE_TRACE_READER
#include <psapi.h>
#include <bit>
#include <chrono>
#include <cstring>
#include <functional>
#include <fstream>
#include <filesystem>
#include <new>
#include <optional>
#include <sstream>

using namespace fidelity;
namespace {
struct Mismatch : std::runtime_error { using std::runtime_error::runtime_error; };
std::string utf8(std::wstring_view text) {
    check(text.size() < 32768,"UTF-8 input size");
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    check(size > 0 || text.empty(),"UTF-8 conversion");
    std::string result(static_cast<size_t>(size),'\0');
    if (size) check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr) == size,"UTF-8 conversion");
    return result;
}
std::wstring psLiteral(std::wstring_view text) {
    std::wstring result=L"'";
    for (wchar_t c : text) { if (c == L'\'') result += L'\''; result += c; }
    return result+L'\'';
}
std::wstring systemProgram(std::wstring_view relative) {
    wchar_t path[MAX_PATH]={}; const UINT count=GetSystemDirectoryW(path,MAX_PATH);
    check(count > 0 && count < MAX_PATH,"system directory");
    return std::wstring(path,count)+L"\\"+std::wstring(relative);
}
class Session {
public:
    VtState state{80,25};
    std::string raw;
    uint64_t lineFeeds=0;
    bool dropOutput=false;
    bool replyToQueries=true;
    std::string decodeError;
    Handle child, agent, descendant;
    explicit Session(bool observeDecodeErrors = false) : observeDecodeErrors_(observeDecodeErrors) {
        vt7pty_error_ptr_t error=nullptr;
        auto *config=vt7pty_config_new(0,&error);
        check(config != nullptr,"session config");
        vt7pty_config_set_initial_size(config,80,25);
        vt7pty_config_set_agent_timeout(config,5000);
        pty_=vt7pty_open(config,&error); vt7pty_config_free(config);
        if (error) { const std::string message=utf8(vt7pty_error_msg(error)); vt7pty_error_free(error);
            if (pty_) { vt7pty_free(pty_); pty_=nullptr; }
            throw std::runtime_error("agent startup: "+message);
        }
        check(pty_ != nullptr,"agent startup");
        try {
            check(DuplicateHandle(GetCurrentProcess(),vt7pty_agent_process(pty_),GetCurrentProcess(),&agent.value,0,FALSE,DUPLICATE_SAME_ACCESS),"agent ownership");
            input_.reset(CreateFileW(vt7pty_conin_name(pty_),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            output_.reset(CreateFileW(vt7pty_conout_name(pty_),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            check(input_.value != INVALID_HANDLE_VALUE && output_.value != INVALID_HANDLE_VALUE,"session pipes");
        } catch (...) { vt7pty_free(pty_); pty_=nullptr; throw; }
    }
    ~Session() { cleanup(); }
    void spawn(const std::wstring &application,const std::wstring &command) {
        DWORD win32=0;
        vt7pty_result_t code=VT7PTY_ERROR_SUCCESS;
        check(trySpawn(application,command,VT7PTY_SPAWN_FLAG_AUTO_SHUTDOWN,win32,code),"spawn");
    }
    bool trySpawn(const std::wstring &application,const std::wstring &command,UINT64 flags,
                  DWORD &win32,vt7pty_result_t &code) {
        vt7pty_error_ptr_t error=nullptr;
        auto *config=vt7pty_spawn_config_new(flags,application.c_str(),command.c_str(),nullptr,nullptr,&error);
        if (!config && error) vt7pty_error_free(error);
        check(config != nullptr,"spawn config");
        const BOOL result=vt7pty_spawn(pty_,config,&child.value,nullptr,&win32,&error);
        vt7pty_spawn_config_free(config);
        code=error ? vt7pty_error_code(error) : VT7PTY_ERROR_SUCCESS;
        if (error) vt7pty_error_free(error);
        return result != FALSE;
    }
    bool outputEof() const { return eof_; }
    vt7pty_t *runtimeHandle() const { return pty_; }
    void input(std::string_view text) {
        check(text.size() <= 4096,"input bound");
        transfer(input_.value,const_cast<char *>(text.data()),static_cast<DWORD>(text.size()),true);
    }
    void pump() {
        if (eof_) return;
        for (int iteration=0;iteration<64;++iteration) {
            DWORD available=0;
            if (!PeekNamedPipe(output_.value,nullptr,0,nullptr,&available,nullptr)) {
                check(GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_PIPE_NOT_CONNECTED,"output peek");
                eof_=true; decode({}); return;
            }
            if (available == 0) return;
            char bytes[4096]={};
            const DWORD length=std::min<DWORD>(available,sizeof(bytes));
            transfer(output_.value,bytes,length,false);
            check(raw.size()+length <= 1024*1024,"captured output bound");
            raw.append(bytes,length);
            lineFeeds+=static_cast<uint64_t>(std::count(bytes,bytes+length,'\n'));
            decode(std::string_view(bytes,length));
            for (const auto &reply : state.takeReplies()) {
                if (replyToQueries) input(reply);
            }
        }
    }
    bool until(const std::function<bool()> &condition,DWORD milliseconds=5000) {
        const ULONGLONG deadline=GetTickCount64()+milliseconds;
        do { pump(); if (condition()) return true; Sleep(2); } while (GetTickCount64()<deadline);
        pump(); return condition();
    }
    void resizeBackend(int columns,int rows) {
        vt7pty_error_ptr_t error = nullptr;
        const BOOL resized = vt7pty_set_size(pty_, columns, rows, &error);
        if (error) {
            const auto message = utf8(vt7pty_error_msg(error));
            vt7pty_error_free(error);
            if (!resized) throw std::runtime_error("resize: " + message);
        }
        check(resized != FALSE, "resize");
    }
    void resize(int columns,int rows) {
        resizeBackend(columns,rows);
        state.resize(columns,rows);
    }
    void finish(DWORD expectedStatus=0) {
        if (!until([&] { return eof_ && WaitForSingleObject(child.value,0) == WAIT_OBJECT_0; },10000))
            throw Mismatch("final output/child completion deadline");
        DWORD status=0; check(GetExitCodeProcess(child.value,&status),"child status");
        if (status != expectedStatus) throw std::runtime_error("fixture/controlled shell exit "+std::to_string(status));
        if (!cleanup()) throw Mismatch("owned process survived shutdown");
    }
    bool cleanup() noexcept {
        if (closed_) return cleanupOkay_;
        closed_=true;
        input_.reset();
        if (pty_) { vt7pty_free(pty_); pty_=nullptr; }
        cleanupOkay_=true;
        for (HANDLE process : {descendant.value,child.value,agent.value}) {
            if (process == INVALID_HANDLE_VALUE || process == nullptr) continue;
            if (WaitForSingleObject(process,5000) != WAIT_OBJECT_0) {
                cleanupOkay_=false;
                TerminateProcess(process,125);
                WaitForSingleObject(process,5000);
            }
        }
        output_.reset();
        return cleanupOkay_;
    }
private:
    void decode(std::string_view bytes) {
        if (dropOutput) return;
        try {
            if (bytes.empty()) state.finish();
            else state.feed(bytes);
        } catch (const std::runtime_error &error) {
            if (!observeDecodeErrors_) throw;
            decodeError = error.what();
            dropOutput = true;
        }
    }
    bool observeDecodeErrors_;
    vt7pty_t *pty_=nullptr;
    Handle input_,output_;
    bool eof_=false,closed_=false,cleanupOkay_=true;
};
uint16_t visualAttribute(WORD value) {
    uint16_t result=static_cast<uint16_t>(value & 0x80ff);
    if (value & COMMON_LVB_REVERSE_VIDEO)
        result=static_cast<uint16_t>((result & 0x8000) | ((result & 15)<<4) | ((result & 240)>>4));
    return result;
}
bool sameScreen(const VtState &state,const Packet &packet) {
    if (packet.columns != static_cast<uint32_t>(state.columns()) || packet.rows != static_cast<uint32_t>(state.rows()) ||
        packet.cursorX != static_cast<uint32_t>(state.x()) || packet.cursorY != static_cast<uint32_t>(state.y()) ||
        (packet.cursorVisible != 0) != state.visible()) return false;
    for (uint32_t y=0;y<packet.rows;++y) for (uint32_t x=0;x<packet.columns;++x) {
        const auto &observed=state.at(static_cast<int>(x),static_cast<int>(y));
        const auto &expected=packet.cells[y*packet.columns+x];
        if (observed.text != std::u32string(1,static_cast<char32_t>(expected.Char.UnicodeChar)) ||
            observed.attributes != visualAttribute(expected.Attributes) || observed.continuation) return false;
    }
    return true;
}
std::u32string snapshotText(const Packet &packet,uint32_t y) {
    std::u32string result;
    for (uint32_t x=0;x<packet.columns;++x) {
        const auto &cell=packet.cells[y*packet.columns+x];
        if (cell.Attributes & COMMON_LVB_TRAILING_BYTE) continue;
        const uint32_t c=cell.Char.UnicodeChar;
        if (c >= 0xd800 && c <= 0xdbff && x+1<packet.columns) {
            uint32_t next=x+1;
            while (next<packet.columns && (packet.cells[y*packet.columns+next].Attributes & COMMON_LVB_TRAILING_BYTE)) ++next;
            if (next<packet.columns) {
                const uint32_t low=packet.cells[y*packet.columns+next].Char.UnicodeChar;
                if (low>=0xdc00 && low<=0xdfff) {
                    result.push_back(static_cast<char32_t>(0x10000+((c-0xd800)<<10)+(low-0xdc00))); x=next; continue;
                }
            }
        }
        result.push_back(static_cast<char32_t>(c));
    }
    while (!result.empty() && result.back() == U' ') result.pop_back();
    return result;
}
std::u32string trimmed(std::u32string text) {
    while (!text.empty() && text.back() == U' ') text.pop_back(); return text;
}
void recipe(const Packet &p) {
    validate(p);
    if (p.columns<40 || p.rows<12) throw std::runtime_error("fixture geometry too small");
    const std::wstring label=p.bufferTag == 2 ? L"STATE_ALT" : L"STATE_READY";
    for (size_t i=0;i<label.size();++i)
        check(p.cells[p.columns+2+i].Char.UnicodeChar == label[i],"fixture label oracle");
    const wchar_t latin[]=L"\u010d\u0107\u017e\u0161\u0111";
    for (size_t i=0;i<5;++i) check(p.cells[4*p.columns+3+i].Char.UnicodeChar == latin[i],"fixture Latin oracle");
    for (size_t i=0;i<5;++i) check(p.cells[6*p.columns+5+i].Attributes == 0x1e,"fixture attribute oracle");
}
std::string packetJson(const Packet &p) {
    return "{\"Columns\":"+std::to_string(p.columns)+",\"Rows\":"+std::to_string(p.rows)+
        ",\"BufferColumns\":"+std::to_string(p.bufferColumns)+",\"BufferRows\":"+std::to_string(p.bufferRows)+
        ",\"WindowLeft\":"+std::to_string(p.windowLeft)+",\"WindowTop\":"+std::to_string(p.windowTop)+
        ",\"CursorX\":"+std::to_string(p.cursorX)+",\"CursorY\":"+std::to_string(p.cursorY)+
        ",\"InputMode\":"+std::to_string(p.inputMode)+",\"OutputMode\":"+std::to_string(p.outputMode)+
        ",\"InputCP\":"+std::to_string(p.inputCP)+",\"OutputCP\":"+std::to_string(p.outputCP)+
        ",\"FontName\":"+json(utf8(std::wstring_view(p.fontName,wcsnlen_s(p.fontName,LF_FACESIZE))))+
        ",\"FontWidth\":"+std::to_string(p.fontWidth)+",\"FontHeight\":"+std::to_string(p.fontHeight)+",\"FontError\":"+std::to_string(p.fontError)+
        ",\"ControlCount\":"+std::to_string(p.controlCount)+",\"KeyCount\":"+std::to_string(p.keyCount)+
        ",\"BufferTag\":"+std::to_string(p.bufferTag)+"}";
}
std::string differenceJson(const VtState &state,const Packet &packet) {
    uint32_t text=0,attribute=0;
    std::string first="null";
    if (packet.columns <= 120 && packet.rows <= 40) {
        for (uint32_t y=0;y<std::min(packet.rows,static_cast<uint32_t>(state.rows()));++y)
            for (uint32_t x=0;x<std::min(packet.columns,static_cast<uint32_t>(state.columns()));++x) {
                const auto &cell=state.at(static_cast<int>(x),static_cast<int>(y));
                const auto &expected=packet.cells[y*packet.columns+x];
                const bool wrongText=cell.text != std::u32string(1,static_cast<char32_t>(expected.Char.UnicodeChar)) || cell.continuation;
                const bool wrongAttribute=cell.attributes != visualAttribute(expected.Attributes);
                text+=wrongText ? 1u : 0u; attribute+=wrongAttribute ? 1u : 0u;
                if ((wrongText || wrongAttribute) && first == "null")
                    first="{\"X\":"+std::to_string(x)+",\"Y\":"+std::to_string(y)+
                        ",\"ExpectedUtf16\":"+std::to_string(expected.Char.UnicodeChar)+
                        ",\"ObservedScalar\":"+std::to_string(cell.text.empty() ? 0u : static_cast<uint32_t>(cell.text[0]))+
                        ",\"ExpectedAttribute\":"+std::to_string(visualAttribute(expected.Attributes))+
                        ",\"ObservedAttribute\":"+std::to_string(cell.attributes)+"}";
            }
    }
    return "{\"CursorX\":"+std::to_string(state.x())+",\"CursorY\":"+std::to_string(state.y())+
        ",\"Columns\":"+std::to_string(state.columns())+",\"Rows\":"+std::to_string(state.rows())+
        ",\"RawUtf16CellDifferences\":"+std::to_string(text)+",\"AttributeDifferences\":"+std::to_string(attribute)+
        ",\"FirstCellDifference\":"+first+"}";
}
struct Observations {
    std::string fields=""; std::string raw;
    std::string console="null", comparison="null";
    bool cleanup=true;
    void add(const std::string &key,bool value) { fields+=","+json(key)+":"+(value ? "true" : "false"); }
};

struct ViewportComparison {
    int rowOffset = 0;
    uint32_t comparedRows = 0, textDifferences = 0, attributeDifferences = 0, uncoveredNonblank = 0;
    bool geometry = false, cursor = false;

    bool matches() const {
        return geometry && cursor && comparedRows && !textDifferences && !attributeDifferences && !uncoveredNonblank;
    }

    std::string jsonFields() const {
        return ",\"ViewportRowOffset\":" + std::to_string(rowOffset) +
            ",\"ComparedRows\":" + std::to_string(comparedRows) +
            ",\"CellDifferences\":" + std::to_string(textDifferences) +
            ",\"AttributeDifferences\":" + std::to_string(attributeDifferences) +
            ",\"UncoveredNonblankCells\":" + std::to_string(uncoveredNonblank) +
            ",\"ViewportGeometryMatches\":" + (geometry ? "true" : "false") +
            ",\"ViewportCursorMatches\":" + (cursor ? "true" : "false") +
            ",\"AlignedScreenMatches\":" + (matches() ? "true" : "false");
    }
};

ViewportComparison compareViewport(const VtState &state, const Packet &packet, uint32_t initialTop) {
    ViewportComparison result;
    result.rowOffset = static_cast<int>(packet.windowTop) - static_cast<int>(initialTop);
    result.geometry = packet.windowLeft == 0 && packet.columns == static_cast<uint32_t>(state.columns()) &&
        packet.rows == static_cast<uint32_t>(state.rows());
    if (!result.geometry) return result;

    result.cursor = static_cast<int>(packet.cursorX) == state.x() &&
        static_cast<int>(packet.cursorY) + result.rowOffset == state.y() && (packet.cursorVisible != 0) == state.visible();
    for (uint32_t y = 0; y < packet.rows; ++y) {
        const int readerRow = static_cast<int>(y) + result.rowOffset;
        const bool covered = readerRow >= 0 && readerRow < state.rows();
        if (covered) ++result.comparedRows;
        for (uint32_t x = 0; x < packet.columns; ++x) {
            const auto &expected = packet.cells[y * packet.columns + x];
            const auto attribute = visualAttribute(expected.Attributes);
            if (!covered) {
                if (expected.Char.UnicodeChar != L' ' || expected.Attributes != 7) ++result.uncoveredNonblank;
                continue;
            }
            const auto &observed = state.at(static_cast<int>(x), readerRow);
            if (observed.text != std::u32string(1, static_cast<char32_t>(expected.Char.UnicodeChar)) || observed.continuation)
                ++result.textDifferences;
            if (observed.attributes != attribute) ++result.attributeDifferences;
        }
    }
    return result;
}

bool viewportResizes(Session &session, Packet &packet, const std::wstring &directory, bool fixedRows,
    const std::function<void(char)> &snapshot, Observations &observations) {
    const uint32_t initialTop = packet.windowTop;
    auto save = [&](const std::wstring &label) {
        writeFile(directory + L"\\console-" + label + L".bin",
            std::string_view(reinterpret_cast<const char *>(&packet), sizeof(Packet)));
    };
    save(L"initial");
    observations.fields += ",\"InitialViewport\":" + packetJson(packet);
    std::string checks = "[";
    bool matched = true;
    for (uint32_t i = 0; i < 2 * viewportResizePairs; ++i) {
        const COORD size = i % 2 == 0 ? screenGrownSize : screenInitialSize;
        const uint32_t markerRow = packet.windowTop + 1;
        const std::wstring label = L"resize-" + std::to_wstring(i + 1);
        session.pump();
        const auto boundary = session.raw.size();
        session.resize(size.X, size.Y);
        snapshot('S');
        save(label + L"-before-repaint");
        const bool markerFixed = markerRow >= packet.windowTop &&
            screenTextAt(packet, 2, markerRow - packet.windowTop, L"STATE_READY", 7);
        const std::string before = packetJson(packet);
        const bool sizeBeforeRepaint = packet.columns == static_cast<uint32_t>(size.X) &&
            packet.rows == static_cast<uint32_t>(size.Y) && packet.bufferColumns == packet.columns;

        snapshot('R');
        recipe(packet);
        save(label);
        ViewportComparison comparison;
        session.until([&] {
            comparison = compareViewport(session.state, packet, initialTop);
            return comparison.matches();
        }, 2000);
        const bool sizeMatches = sizeBeforeRepaint && packet.columns == static_cast<uint32_t>(size.X) &&
            packet.rows == static_cast<uint32_t>(size.Y) && packet.bufferColumns == packet.columns;
        matched = matched && comparison.matches() && sizeMatches && (!fixedRows || markerFixed);
        if (i) checks += ',';
        checks += "{\"Columns\":" + std::to_string(size.X) + ",\"Rows\":" + std::to_string(size.Y) +
            ",\"CapturedBytesBefore\":" + std::to_string(boundary) + ",\"CapturedBytes\":" +
            std::to_string(session.raw.size()) + ",\"BeforeRepaint\":" + before + ",\"Console\":" + packetJson(packet) +
            ",\"DecodedState\":" + differenceJson(session.state, packet) +
            ",\"ReconstructedCursorVisible\":" + (session.state.visible() ? "true" : "false") +
            ",\"FixedMarkerBeforeRepaint\":" + (markerFixed ? "true" : "false") +
            ",\"RequestedSizeMatches\":" + (sizeMatches ? "true" : "false") + comparison.jsonFields() + '}';
    }
    observations.fields += ",\"ViewportResizes\":" + checks + ']';
    return matched;
}

std::string pointList(std::u32string_view text) {
    std::string result = "[";
    for (const auto value : text) {
        if (result.size() > 1) result += ',';
        result += std::to_string(static_cast<uint32_t>(value));
    }
    return result + ']';
}

std::u32string outputBarrier(uint32_t phase) {
    const auto marker = outputBarriers[phase - 1];
    return std::u32string(marker.begin(), marker.end());
}

std::unique_ptr<Packet> outputRecipe(const OutputCase &scenario, uint32_t phase, bool applicable) {
    auto expected = std::make_unique<Packet>();
    expected->columns = outputColumns;
    expected->rows = outputRows;
    expected->cursorVisible = 1;
    for (int i = 0; i < outputColumns * outputRows; ++i) {
        expected->cells[i].Char.UnicodeChar = L' ';
        expected->cells[i].Attributes = 7;
    }
    const auto row = [&](int x, int y, std::wstring_view text, WORD attribute) {
        for (const auto value : text) {
            auto &cell = expected->cells[y * outputColumns + x++];
            cell.Char.UnicodeChar = value;
            cell.Attributes = attribute;
        }
    };
    row(0, outputBarrierRow, outputBarriers[phase - 1], 7);
    switch (scenario.action) {
    case OutputAction::Position:
        if (phase == 2) {
            row(5, 6, L"COLOR", 0x1e);
            row(13, 6, L"RED", 0x4f);
            expected->cursorX = 17;
            expected->cursorY = 7;
        }
        break;
    case OutputAction::Erase:
        row(4, 5, L"ABCDE", 0x1e);
        if (phase == 2) {
            row(5, 5, L"  ", 0x20);
            expected->cursorX = 5;
            expected->cursorY = 5;
        }
        break;
    case OutputAction::Wrap:
        if (phase == 2) {
            row(outputColumns - 2, 7, L"WX", 7);
            row(0, 8, L"YZ", 7);
            expected->cursorX = 2;
            expected->cursorY = 8;
        }
        break;
    case OutputAction::Scroll:
        row(8, 3, L"GUARD", 7);
        row(8, 4, phase == 1 ? L"FIRST" : L"SECOND", phase == 1 ? 0x1e : 0x2f);
        row(8, 5, phase == 1 ? L"SECOND" : L"THIRD", phase == 1 ? 0x2f : 0x4e);
        row(8, 6, phase == 1 ? L"THIRD" : L"      ", phase == 1 ? 0x4e : 0x20);
        if (phase == 2) {
            expected->cursorX = 8;
            expected->cursorY = 6;
        }
        break;
    case OutputAction::RawVT:
        if (phase == 2 && applicable) {
            row(3, 8, L"V", 0x14);
            expected->cursorX = 4;
            expected->cursorY = 8;
        }
        break;
    case OutputAction::WideText:
    case OutputAction::ByteText:
        break;
    }
    return expected;
}

bool consoleMatchesRecipe(const Packet &actual, const Packet &expected) {
    return actual.columns == expected.columns && actual.rows == expected.rows &&
        actual.cursorX == expected.cursorX && actual.cursorY == expected.cursorY &&
        actual.cursorVisible == expected.cursorVisible &&
        std::equal(actual.cells, actual.cells + actual.rows * actual.columns, expected.cells,
            [](const CHAR_INFO &a, const CHAR_INFO &b) {
                return a.Char.UnicodeChar == b.Char.UnicodeChar && visualAttribute(a.Attributes) == visualAttribute(b.Attributes);
            });
}

const char *boolean(bool value) { return value ? "true" : "false"; }

int trailingBarColumn(const Packet &packet) {
    int column = -1;
    for (int x = 0; x < outputColumns; ++x) {
        if (packet.cells[outputTextY * outputColumns + x].Char.UnicodeChar == L'|') column = x;
    }
    return column;
}

int trailingBarColumn(const VtState &state) {
    int column = -1;
    for (int x = 0; x < outputColumns; ++x) {
        if (state.at(x, outputTextY).text == U"|") column = x;
    }
    return column;
}

std::string replayStatus(std::string_view status) {
    return "{\"Status\":" + json(status) + "}";
}

std::string describeWidthReplay(const VtState &state, const Packet &packet, std::u32string native, int nativeBar) {
    auto text = trimmed(state.row(outputTextY));
    const int bar = trailingBarColumn(state);
    const bool exact = text == native;
    const auto points = pointList(text);
    std::erase(text, U' ');
    std::erase(native, U' ');
    return "{\"Text\":" + points +
        ",\"TextMatchesNative\":" + boolean(exact) +
        ",\"NonblankTextMatchesNative\":" + boolean(text == native) +
        ",\"BarColumn\":" + std::to_string(bar) +
        ",\"BarMatchesNative\":" + boolean(bar == nativeBar) +
        ",\"CursorMatchesNative\":" + boolean(state.x() == static_cast<int>(packet.cursorX) &&
            state.y() == static_cast<int>(packet.cursorY) && state.visible() == (packet.cursorVisible != 0)) + "}";
}

std::string replayWidth(const OutputCase &scenario, const Packet &packet, std::string_view prefix, size_t actionStart) {
    check(packet.columns == outputColumns && packet.rows == outputRows && actionStart <= prefix.size(),
        "width replay geometry and stream boundary");
    const auto native = snapshotText(packet, outputTextY);
    std::u32string expected(outputTextX, U' ');
    expected += scenario.text;
    const int nativeBar = trailingBarColumn(packet);
    if (native != expected || nativeBar < 0) return replayStatus("NativeSampleChanged");

    const auto sample = utf8(scenario.wide);
    const auto match = prefix.find(sample, actionStart);
    if (match == std::string_view::npos || prefix.find(sample, match + sample.size()) != std::string_view::npos)
        return replayStatus("PayloadUnavailable");
    const auto offset = match + sample.size() - 1;
    const auto anchor = "\x1b[" + std::to_string(nativeBar + 1) + "G";

    VtState reference(outputColumns, outputRows);
    reference.feed(prefix);
    reference.finish();
    VtState oneCell(outputColumns, outputRows, VtState::WidthPolicy::OneCellSamples);
    oneCell.feed(prefix);
    oneCell.finish();
    VtState anchored(outputColumns, outputRows);
    anchored.feed(prefix.substr(0, offset));
    anchored.feed(anchor);
    anchored.feed(prefix.substr(offset));
    anchored.finish();
    return "{\"Status\":\"ReplayComplete\",\"NativeBarColumn\":" + std::to_string(nativeBar) +
        ",\"InsertionOffset\":" + std::to_string(offset) +
        ",\"InsertedSequence\":" + json(anchor) +
        ",\"Reference\":" + describeWidthReplay(reference, packet, native, nativeBar) +
        ",\"OneCellSamples\":" + describeWidthReplay(oneCell, packet, native, nativeBar) +
        ",\"Anchored\":" + describeWidthReplay(anchored, packet, native, nativeBar) + "}";
}

std::string readWidthArtifact(const std::wstring &path, size_t limit) {
    std::ifstream file{std::filesystem::path(path), std::ios::binary | std::ios::ate};
    check(file.is_open(), "width replay evidence file");
    const auto size = file.tellg();
    check(size > 0 && static_cast<uint64_t>(size) <= limit, "width replay evidence size");
    std::string bytes(static_cast<size_t>(size), '\0');
    file.seekg(0);
    check(static_cast<bool>(file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))), "read width evidence");
    return bytes;
}

int replayWidthFromFiles(wchar_t **argv) {
    const auto *scenario = findOutputCase(argv[2]);
    check(scenario && scenario->widthProbe, "width replay case");
    const std::wstring folder = argv[3];
    const auto snapshot = readWidthArtifact(folder + L"\\console-2.bin", sizeof(Packet));
    check(snapshot.size() == sizeof(Packet), "width replay snapshot size");
    auto packet = std::make_unique<Packet>();
    std::memcpy(packet.get(), snapshot.data(), sizeof(Packet));
    validate(*packet);
    const auto stream = readWidthArtifact(folder + L"\\stream.bin", 1024 * 1024);
    const auto start = std::stoull(argv[4]);
    const auto end = std::stoull(argv[5]);
    check(start < end && end <= stream.size(), "width replay checkpoint range");
    const auto result = replayWidth(*scenario, *packet, std::string_view(stream).substr(0, static_cast<size_t>(end)),
        static_cast<size_t>(start));
    puts(result.c_str());
    return 0;
}

struct OutputCheckpoint {
    bool nativeDifference = false;
    bool reconstructedDifference = false;
    bool unavailable = false;
    std::string report;
};

OutputCheckpoint compareOutput(const OutputCase &scenario, const OutputOperation &op, const Packet &packet,
    const Session &session, bool barrierSeen, bool settled) {
    OutputCheckpoint result;
    const auto &state = session.state;
    const bool cursor = state.x() == static_cast<int>(packet.cursorX) && state.y() == static_cast<int>(packet.cursorY) &&
        state.visible() == (packet.cursorVisible != 0);
    const bool applicable = !(op.requestedCP || op.requestedMode) ||
        (op.setupSucceeded && (!op.requestedCP || packet.outputCP == op.requestedCP) &&
            (!op.requestedMode || (packet.outputMode & op.requestedMode) == op.requestedMode));
    result.unavailable = !applicable;
    const auto expected = outputRecipe(scenario, op.phase, applicable);
    bool compared = false;
    std::string metrics;
    if (op.phase == 2 && scenario.isText()) {
        const auto native = snapshotText(packet, outputTextY);
        const auto rendered = trimmed(state.row(outputTextY));
        const bool hasExpected = applicable && !scenario.text.empty();
        std::u32string wanted(outputTextX, U' ');
        wanted += scenario.text;
        const bool nativeTextMatch = native == wanted;
        const int nativeBar = trailingBarColumn(packet);
        const int renderedBar = trailingBarColumn(state);
        const int expectedBar = outputTextX + scenario.width - 1;
        const bool nativeWidthMatch = packet.cursorX == static_cast<uint32_t>(expectedBar + 1) &&
            packet.cursorY == outputTextY && nativeBar == expectedBar;
        result.nativeDifference = hasExpected && (!op.writeSucceeded || !nativeTextMatch || !nativeWidthMatch);
        compared = native == rendered && cursor && nativeBar == renderedBar;
        metrics = ",\"ExpectedText\":" + (hasExpected ? pointList(wanted) : "null") +
            ",\"NativeText\":" + pointList(native) + ",\"ReconstructedText\":" + pointList(rendered) +
            ",\"NativeTextMatchesIntent\":" + (hasExpected ? boolean(nativeTextMatch) : "null") +
            ",\"ExpectedCursorAdvance\":" + (hasExpected ? std::to_string(scenario.width) : "null") +
            ",\"NativeCursorAdvance\":" + std::to_string(static_cast<int>(packet.cursorX) - outputTextX) +
            ",\"ReconstructedCursorAdvance\":" + std::to_string(state.x() - outputTextX) +
            ",\"NativeWidthMatchesIntent\":" + (hasExpected ? boolean(nativeWidthMatch) : "null") +
            ",\"ExpectedTrailingBarColumn\":" + (hasExpected ? std::to_string(expectedBar) : "null") +
            ",\"NativeTrailingBarColumn\":" + std::to_string(nativeBar) +
            ",\"ReconstructedTrailingBarColumn\":" + std::to_string(renderedBar) +
            ",\"TrailingBarColumnMatchesNative\":" + boolean(nativeBar == renderedBar) +
            ",\"ReconstructedTextMatchesNative\":" + boolean(native == rendered);
    } else {
        result.nativeDifference = applicable && (!consoleMatchesRecipe(packet, *expected) ||
            (op.writeAttempted && !op.writeSucceeded));
        compared = sameScreen(state, packet);
        metrics = ",\"ExpectedCursorX\":" + std::to_string(expected->cursorX) +
            ",\"ExpectedCursorY\":" + std::to_string(expected->cursorY);
    }
    result.reconstructedDifference = !barrierSeen || !settled || !session.decodeError.empty() || !compared;
    result.report = "{\"Phase\":" + std::to_string(op.phase) +
        ",\"RequestedOutputCP\":" + std::to_string(op.requestedCP) +
        ",\"RequestedOutputMode\":" + std::to_string(op.requestedMode) +
        ",\"SetupSucceeded\":" + boolean(op.setupSucceeded != 0) + ",\"SetupError\":" + std::to_string(op.setupError) +
        ",\"WriteAttempted\":" + boolean(op.writeAttempted != 0) +
        ",\"WriteSucceeded\":" + boolean(op.writeSucceeded != 0) + ",\"WriteError\":" + std::to_string(op.writeError) +
        ",\"UnitsRequested\":" + std::to_string(op.unitsRequested) + ",\"UnitsWritten\":" + std::to_string(op.unitsWritten) +
        ",\"CapabilityUnavailable\":" + boolean(result.unavailable) +
        ",\"BackingConsoleDiffers\":" + boolean(result.nativeDifference) +
        ",\"ReconstructionDiffers\":" + boolean(result.reconstructedDifference) +
        ",\"BarrierSeen\":" + boolean(barrierSeen) + ",\"Settled\":" + boolean(settled) +
        ",\"CapturedBytes\":" + std::to_string(session.raw.size()) + ",\"ReaderError\":" + json(session.decodeError) +
        ",\"ReconstructedCursorMatchesNative\":" + boolean(cursor) +
        ",\"Console\":" + packetJson(packet) + ",\"Reconstructed\":" + differenceJson(state, packet) + metrics + '}';
    return result;
}

void outputCase(const OutputCase &scenario, const std::wstring &resultDirectory, Observations &observations) {
    Session session(true);
    std::string checkpoints;
    std::string widthReplay;
    size_t actionStart = 0;
    bool initialFrameReady = false;
    bool consoleDifference = false;
    bool reconstructionDifference = false;
    bool unavailable = false;
    const auto save = [&] {
        const bool validUtf8 = !session.raw.empty() &&
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, session.raw.data(),
                static_cast<int>(session.raw.size()), nullptr, 0) != 0;
        observations.raw = session.raw;
        observations.add("CompleteStreamValidUtf8", validUtf8);
        observations.fields += ",\"ReaderError\":" + json(session.decodeError) +
            ",\"OutputCheckpoints\":[" + checkpoints + "],\"BackingConsoleDiffers\":" + boolean(consoleDifference) +
            ",\"ReconstructionDiffers\":" + boolean(reconstructionDifference || !session.decodeError.empty()) +
            ",\"CapabilityUnavailable\":" + boolean(unavailable);
        if (scenario.widthProbe)
            observations.fields += ",\"WidthReplay\":" + (widthReplay.empty() ? replayStatus("FrameUnavailable") : widthReplay);
        return validUtf8;
    };
    try {
        const auto pipeName = L"\\\\.\\pipe\\VT7Pty.Output." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::to_wstring(GetTickCount64());
        Handle pipe(privateServer(pipeName));
        const auto fixture = directory() + L"\\FidelityFixture.exe";
        session.spawn(fixture, quote(fixture) + L" " + std::wstring(scenario.name) + L" " + quote(pipeName) +
            L" " + std::to_wstring(GetCurrentProcessId()));
        connect(pipe.value);
        ULONG peer = 0;
        check(GetNamedPipeClientProcessId(pipe.value, &peer) && peer == GetProcessId(session.child.value), "output fixture PID");

        std::vector<wchar_t> path(32768);
        const auto client = GetModuleHandleW(L"VT7Pty.dll");
        const DWORD clientLength = client ? GetModuleFileNameW(client, path.data(), static_cast<DWORD>(path.size())) : 0;
        check(clientLength != 0 && clientLength < path.size() &&
            _wcsicmp(path.data(), (directory() + L"\\VT7Pty.dll").c_str()) == 0, "packaged client identity");
        DWORD length = static_cast<DWORD>(path.size());
        check(QueryFullProcessImageNameW(session.agent.value, 0, path.data(), &length) &&
            _wcsicmp(path.data(), (directory() + L"\\VT7Pty-Agent.exe").c_str()) == 0, "packaged agent identity");

        auto packet = std::make_unique<Packet>();
        for (uint32_t phase = 1; phase <= 2; ++phase) {
            OutputOperation op;
            session.pump();
            transfer(pipe.value, &op, sizeof(op), false);
            transfer(pipe.value, packet.get(), sizeof(Packet), false);
            validate(*packet);
            check(op.phase == phase && packet->columns == outputColumns && packet->rows == outputRows &&
                snapshotText(*packet, outputBarrierRow) == outputBarrier(phase), "native output checkpoint identity");
            writeFile(resultDirectory + L"\\console-" + std::to_wstring(phase) + L".bin",
                std::string_view(reinterpret_cast<const char *>(packet.get()), sizeof(Packet)));
            observations.console = packetJson(*packet);
            bool seen = false;
            size_t bytes = session.raw.size();
            ULONGLONG changed = GetTickCount64();
            const auto marker = outputBarrier(phase);
            const bool settled = session.until([&] {
                const auto now = GetTickCount64();
                if (bytes != session.raw.size()) {
                    bytes = session.raw.size();
                    changed = now;
                }
                seen = trimmed(session.state.row(outputBarrierRow)) == marker;
                return (seen || !session.decodeError.empty()) && now - changed >= 80;
            }, 4000);
            const auto checkpoint = compareOutput(scenario, op, *packet, session, seen, settled);
            if (scenario.widthProbe) {
                const bool frameReady = seen && settled && session.decodeError.empty();
                if (phase == 1) {
                    actionStart = session.raw.size();
                    initialFrameReady = frameReady;
                } else if (checkpoint.unavailable) {
                    widthReplay = replayStatus("CapabilityUnavailable");
                } else if (!initialFrameReady || !frameReady) {
                    widthReplay = replayStatus("FrameUnavailable");
                } else {
                    widthReplay = replayWidth(scenario, *packet, session.raw, actionStart);
                }
            }
            consoleDifference = consoleDifference || checkpoint.nativeDifference;
            reconstructionDifference = reconstructionDifference || checkpoint.reconstructedDifference;
            unavailable = unavailable || checkpoint.unavailable;
            if (!checkpoints.empty()) checkpoints += ',';
            checkpoints += checkpoint.report;
            char command = phase == 1 ? 'A' : 'Q';
            transfer(pipe.value, &command, 1, true);
        }
        session.finish();
    } catch (...) {
        observations.cleanup = session.cleanup();
        save();
        throw;
    }
    observations.cleanup = session.cleanup();
    const bool utf8Valid = save();
    if (consoleDifference || reconstructionDifference || !session.decodeError.empty() || !utf8Valid)
        throw Mismatch("output characterization difference");
}

void fixtureCase(const std::wstring &name,const std::wstring &resultDirectory,Observations &observations) {
    Session session;
    auto packetStorage=std::make_unique<Packet>();
    Packet &packet=*packetStorage;
    const bool fixedRows = name == L"SCREEN_FIXED_ROWS";
    const bool viewport = fixedRows || name == L"SCREEN_VIEWPORT";
    bool viewportMatches = true;
    try {
        const std::wstring pipeName=L"\\\\.\\pipe\\VT7Pty.Fidelity."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        Handle pipe(privateServer(pipeName));
        std::wstring mode=L"STATE";
        if (fixedRows) mode=L"FIXED_ROWS";
        if (name == L"UNICODE") mode=L"UNICODE";
        if (name == L"INPUT" || name == L"UNPROCESSED_CTRL_C" || name == L"NEGATIVE_DUPLICATE") mode=L"INPUT";
        if (name == L"PROCESSED_CTRL_C") mode=L"PROCESSED";
        const std::wstring fixture=directory()+L"\\FidelityFixture.exe";
        session.spawn(fixture,quote(fixture)+L" "+mode+L" "+quote(pipeName)+L" "+std::to_wstring(GetCurrentProcessId()));
        connect(pipe.value);
        ULONG peer=0; check(GetNamedPipeClientProcessId(pipe.value,&peer) && peer == GetProcessId(session.child.value),"fixture client PID");
        transfer(pipe.value,&packet,sizeof(packet),false); recipe(packet);
        observations.console=packetJson(packet);
        writeFile(resultDirectory+L"\\console-snapshot.bin",std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
        auto snapshot=[&](char command='S') {
            transfer(pipe.value,&command,1,true);
            transfer(pipe.value,&packet,sizeof(packet),false); validate(packet);
            observations.console=packetJson(packet);
            writeFile(resultDirectory+L"\\console-snapshot.bin",std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
        };
        const ULONGLONG begin=GetTickCount64();
        if (mode == L"INPUT" || mode == L"PROCESSED") {
            if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("input fixture initial screen mismatch");
            if (name == L"PROCESSED_CTRL_C" || name == L"UNPROCESSED_CTRL_C") session.input("\x03z");
            else {
                const std::string input="A\xc4\x87\x1b[D\x7f";
                for (char c : input) session.input(std::string_view(&c,1));
            }
            const uint32_t expected=name == L"PROCESSED_CTRL_C" ? 1u :
                (name == L"UNPROCESSED_CTRL_C" ? 2u : 4u);
            if (!session.until([&] { snapshot(); return packet.keyCount >= expected &&
                (name != L"PROCESSED_CTRL_C" || packet.controlCount >= 1); })) throw Mismatch("input delivery deadline");
            if (name == L"NEGATIVE_DUPLICATE") { check(packet.keyCount < 128,"duplicate fault count bound"); packet.keys[packet.keyCount++]=packet.keys[0]; }
            bool correct=packet.keyCount == expected;
            for (uint32_t index=0;index<packet.keyCount;++index) correct=correct && packet.keys[index].repeat == 1;
            if (name == L"PROCESSED_CTRL_C")
                correct=correct && packet.controlCount == 1 && packet.keys[0].character == 'z';
            else if (name == L"UNPROCESSED_CTRL_C")
                correct=correct && packet.controlCount == 0 && packet.keys[0].character == 3 &&
                    (packet.keys[0].modifiers & LEFT_CTRL_PRESSED) && packet.keys[1].character == 'z';
            else correct=correct && packet.keys[0].character == 'A' && packet.keys[1].character == 0x107 &&
                packet.keys[2].virtualKey == VK_LEFT && packet.keys[3].virtualKey == VK_BACK;
            observations.add("InputMatches",correct);
            if (!correct) throw Mismatch("input/control count or record mismatch");
        } else if (name == L"UNICODE") {
            const auto expected=snapshotText(packet,9);
            session.until([&] { return trimmed(session.state.row(9)) == expected &&
                packet.cursorX == static_cast<uint32_t>(session.state.x()) &&
                packet.cursorY == static_cast<uint32_t>(session.state.y()); });
            const bool text=trimmed(session.state.row(9)) == expected;
            const bool cursor=packet.cursorX == static_cast<uint32_t>(session.state.x()) &&
                packet.cursorY == static_cast<uint32_t>(session.state.y());
            observations.add("UnicodeTextMatches",text); observations.add("UnicodeCursorMatches",cursor);
            if (!text || !cursor) throw Mismatch("Unicode text/cursor width differs from backing console");
        } else {
            if (name == L"NEGATIVE_PACKET") { packet.magic=0; validate(packet); }
            if (name == L"NEGATIVE_CELL") packet.cells[packet.columns+2].Char.UnicodeChar=L'!';
            if (name == L"NEGATIVE_ATTRIBUTE") packet.cells[6*packet.columns+5].Attributes=7;
            if (name == L"NEGATIVE_CURSOR") ++packet.cursorX;
            if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("screen/cursor/attribute mismatch");
            if (name == L"ALTERNATE") {
                snapshot('A'); recipe(packet);
                if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("alternate screen mismatch");
                snapshot('M'); recipe(packet);
                if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("restored primary mismatch");
            }
            if (name == L"RESIZE") {
                for (const auto size : {COORD{100,30},COORD{80,25},COORD{100,30},COORD{80,25}}) {
                    session.resize(size.X,size.Y); snapshot('R'); recipe(packet);
                    check(packet.columns == static_cast<uint32_t>(size.X) && packet.rows == static_cast<uint32_t>(size.Y),"child resize dimensions");
                    if (!session.until([&] { return sameScreen(session.state,packet); })) throw Mismatch("resized screen mismatch");
                }
            }
            if (viewport) viewportMatches = viewportResizes(session, packet, resultDirectory, fixedRows, snapshot, observations);
            if (name == L"NEGATIVE_TIMEOUT" && !session.until([] { return false; },1000))
                throw Mismatch("deliberate readiness deadline");
            if (name == L"NEGATIVE_ORPHAN" && WaitForSingleObject(session.child.value,0) != WAIT_OBJECT_0)
                throw Mismatch("deliberately live child detected before cleanup");
        }
        observations.comparison=differenceJson(session.state,packet);
        observations.fields+=",\"ObservationMilliseconds\":"+std::to_string(GetTickCount64()-begin);
        if (name == L"NEGATIVE_DRAIN") session.dropOutput=true;
        char quit='Q'; transfer(pipe.value,&quit,1,true); session.finish();
        const bool drain=session.state.contains(U"FINAL_DRAIN");
        observations.add("FinalDrainMatches",drain);
        if (!drain) throw Mismatch("final marker missing from decoded output");
        if (!viewportMatches) throw Mismatch("stable resize viewport or fixed marker differs");
    } catch (...) {
        observations.comparison=differenceJson(session.state,packet);
        observations.raw=session.raw; observations.cleanup=session.cleanup(); throw;
    }
    observations.raw=session.raw; observations.cleanup=session.cleanup();
}
std::u32string startShell(const std::wstring &name,const std::wstring &directoryPath,const std::wstring &program,
    Session &session,Observations &observations) {
    const bool cmd=name.starts_with(L"CMD");
    const std::wstring executable=cmd ? systemProgram(L"cmd.exe") :
        (name.starts_with(L"PS7") ? program : systemProgram(L"WindowsPowerShell\\v1.0\\powershell.exe"));
    check(!executable.empty(),"shell executable path");
    const std::u32string expectedPrompt=cmd ? U"FIDELITY_CMD_PROMPT>" : U"FIDELITY_PS_PROMPT>";
    std::wstring boot=name.starts_with(L"PS7") ? L"function global:prompt { 'FIDELITY_PS_PROMPT>' }" :
        L"Remove-Module PSReadLine -ErrorAction SilentlyContinue; function global:prompt { 'FIDELITY_PS_PROMPT>' }";
    if (name.starts_with(L"PS7")) {
        const std::wstring module=(std::filesystem::path(executable).parent_path()/L"Modules"/L"PSReadLine"/L"PSReadLine.psd1").wstring();
        check(GetFileAttributesW(module.c_str()) != INVALID_FILE_ATTRIBUTES,"PowerShell 7 case requires bundled PSReadLine");
        boot=L"Import-Module "+psLiteral(module)+L" -ErrorAction Stop; Set-PSReadLineOption -HistorySavePath "+
            psLiteral(directoryPath+L"\\psreadline-history.txt")+
            L" -HistorySaveStyle SaveNothing -PredictionSource None -EditMode Windows; "+boot;
    }
    const std::wstring command=cmd ? quote(executable)+L" /D /Q /K \"prompt FIDELITY_CMD_PROMPT$G\"" :
        quote(executable)+L" -NoLogo -NoProfile -NoExit -Command "+quote(boot);
    session.spawn(executable,command);
    auto atPrompt=[&] { return trimmed(session.state.row(session.state.y())) == expectedPrompt; };
    if (!session.until(atPrompt,20000)) throw std::runtime_error("controlled shell prompt unavailable");
    if (!cmd) {
        const std::wstring identityCommand=L"$m=Get-Module PSReadLine; $mv='none'; $cfg='native-ConsoleHost'; if($m){$mv=$m.Version.ToString(); $o=Get-PSReadLineOption; $cfg=$m.Path+'|'+$o.EditMode.ToString()+'|'+$o.HistorySaveStyle.ToString()}; [IO.File]::WriteAllText("+
            psLiteral(directoryPath+L"\\shell-identity.txt")+L",$PSVersionTable.PSVersion.ToString()+'|'+$mv+'|'+$Host.Name+'|'+$cfg)";
        const uint64_t lines=session.lineFeeds;
        session.input(utf8(identityCommand+L"\r"));
        if (!session.until([&] { return session.lineFeeds>lines && atPrompt() &&
            GetFileAttributesW((directoryPath+L"\\shell-identity.txt").c_str()) != INVALID_FILE_ATTRIBUTES; },10000))
            throw std::runtime_error("shell identity barrier");
        std::ifstream identity(std::filesystem::path(directoryPath+L"\\shell-identity.txt"));
        std::string metadata; std::getline(identity,metadata);
        check(!metadata.empty(),"shell metadata");
        observations.fields+=",\"ShellIdentity\":"+json(metadata);
        if (name.starts_with(L"PS7") && (!metadata.starts_with("7.2.24|") || metadata.find("|none|") != std::string::npos))
            throw std::runtime_error("PowerShell 7 case requires engine 7.2.24 and loaded PSReadLine");
        if (name.starts_with(L"PS_") && metadata.find("|none|ConsoleHost|native-ConsoleHost") == std::string::npos)
            throw std::runtime_error("Windows PowerShell case requires native ConsoleHost editing");
    }
    return expectedPrompt;
}

std::string childJson(const ChildPacket &p) {
    std::string keys="[";
    for (uint32_t i=0;i<p.keyCount;++i) {
        if (i) keys+=",";
        keys+="{\"Character\":"+std::to_string(p.keys[i].character)+",\"VirtualKey\":"+std::to_string(p.keys[i].virtualKey)+
            ",\"Modifiers\":"+std::to_string(p.keys[i].modifiers)+",\"Repeat\":"+std::to_string(p.keys[i].repeat)+"}";
    }
    return "{\"Stage\":"+std::to_string(p.stage)+",\"ProcessId\":"+std::to_string(p.processId)+
        ",\"ParentId\":"+std::to_string(p.parentId)+",\"InputMode\":"+std::to_string(p.inputMode)+
        ",\"ControlCount\":"+std::to_string(p.controlCount)+",\"ControlType\":"+std::to_string(p.controlType)+
        ",\"ReadReturned\":"+std::to_string(p.readReturned)+",\"ReadSucceeded\":"+std::to_string(p.readSucceeded)+
        ",\"ReadError\":"+std::to_string(p.readError)+",\"Text\":"+json(utf8(std::wstring_view(p.text,p.textCount)))+
        ",\"Keys\":"+keys+"]}";
}
bool receiveChild(HANDLE pipe,ChildPacket &packet) {
    DWORD available=0;
    if (!PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)) {
        const DWORD error=GetLastError();
        check(error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED,"child observation peek");
        return false;
    }
    if (available<sizeof(packet)) return false;
    transfer(pipe,&packet,sizeof(packet),false); validateChild(packet); return true;
}
void saveChild(const std::wstring &directoryPath,const wchar_t *name,const ChildPacket &packet) {
    writeFile(directoryPath+L"\\"+name,std::string_view(reinterpret_cast<const char *>(&packet),sizeof(packet)));
}
std::string fileText(const std::wstring &path) {
    std::ifstream file{std::filesystem::path(path)};
    std::string text; std::getline(file,text);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) text.pop_back();
    return text;
}

void openShellFixture(Session &session, HANDLE pipe, const std::wstring &fixture) {
    connect(pipe);
    ULONG peer = 0;
    check(GetNamedPipeClientProcessId(pipe, &peer), "shell fixture peer PID");
    Handle candidate(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, peer));
    check(candidate.value != nullptr, "shell fixture process");
    check(parentProcessId(peer) == GetProcessId(session.child.value), "fixture parent is tested shell");
    std::vector<wchar_t> image(32768);
    DWORD length = static_cast<DWORD>(image.size());
    check(QueryFullProcessImageNameW(candidate.value, 0, image.data(), &length) &&
        _wcsicmp(image.data(), fixture.c_str()) == 0, "shell fixture image");
    // Adopt teardown ownership only after both ancestry and image validation.
    session.descendant.reset(candidate.value);
    candidate.value = INVALID_HANDLE_VALUE;
}

std::wstring shellFixtureCommand(bool cmd, const std::wstring &fixture, std::wstring_view mode,
    const std::wstring &pipeName) {
    const auto serverId = std::to_wstring(GetCurrentProcessId());
    if (cmd) return quote(fixture) + L" " + std::wstring(mode) + L" " + quote(pipeName) + L" " + serverId;
    return L"& " + psLiteral(fixture) + L" " + psLiteral(mode) + L" " + psLiteral(pipeName) + L" " + serverId;
}

std::string screenGeometryJson(const ScreenActivity &size) {
    return "{\"Columns\":" + std::to_string(size.columns) + ",\"Rows\":" + std::to_string(size.rows) +
        ",\"BufferColumns\":" + std::to_string(size.bufferColumns) + ",\"BufferRows\":" + std::to_string(size.bufferRows) +
        ",\"WindowLeft\":" + std::to_string(size.windowLeft) + ",\"WindowTop\":" + std::to_string(size.windowTop) +
        ",\"Draws\":" + std::to_string(size.draws) + ",\"WindowEvents\":" + std::to_string(size.windowEvents) +
        ",\"EventColumns\":" + std::to_string(size.eventColumns) + ",\"EventRows\":" + std::to_string(size.eventRows) + '}';
}

bool screenEditMatches(const Packet &packet, std::wstring_view text) {
    if (packet.keyCount != text.size()) return false;
    for (uint32_t i = 0; i < packet.keyCount; ++i)
        if (packet.keys[i].character != static_cast<uint32_t>(text[i]) || packet.keys[i].repeat != 1) return false;
    return true;
}

void screenCase(const ScreenCase &scenario, const std::wstring &resultDirectory, Observations &observations) {
    Session session;
    Handle pipe;
    auto packet = std::make_unique<Packet>();
    std::string checkpoints = "[", resizes = "[";
    bool matched = true;
    bool completed = false;
    auto add = [&](const char *name, bool condition) {
        observations.add(name, condition);
        matched = matched && condition;
    };
    auto finishEvidence = [&] {
        observations.fields += ",\"ScreenCheckpoints\":" + checkpoints + "],\"ResizeChecks\":" + resizes + ']';
        observations.add("ScreenWorkflowCompleted", completed);
        pipe.reset();
        observations.raw = session.raw;
        observations.cleanup = session.cleanup();
    };
    auto send = [&](char command) { transfer(pipe.value, &command, 1, true); };
    auto resize = [&](COORD size) {
        try {
            session.resize(size.X, size.Y);
        } catch (const std::runtime_error &error) {
            DWORD code = 0;
            check(GetExitCodeProcess(session.agent.value, &code), "read agent status after resize failure");
            observations.fields += ",\"FailedResize\":{\"Columns\":" + std::to_string(size.X) +
                ",\"Rows\":" + std::to_string(size.Y) + ",\"CapturedBytesBefore\":" +
                std::to_string(session.raw.size()) + ",\"AgentExitCode\":" + std::to_string(code) +
                ",\"Error\":" + json(error.what()) + '}';
            throw Mismatch(error.what());
        }
    };
    auto capture = [&](const wchar_t *label, COORD size, bool alternate, std::wstring_view edit = L"") {
        ScreenActivity activity;
        transfer(pipe.value, packet.get(), sizeof(Packet), false);
        transfer(pipe.value, &activity, sizeof(activity), false);
        validate(*packet);
        writeFile(resultDirectory + L"\\console-" + label + L".bin",
            std::string_view(reinterpret_cast<const char *>(packet.get()), sizeof(Packet)));
        const bool afterResize = scenario.exit == ScreenExit::Resize && std::wstring_view(label) == L"after-exit";
        const auto primaryMarker = primaryMarkerPlacement(*packet, afterResize);
        const bool marker = alternate ? screenTextAt(*packet, 2, 1, screenAlternate, 0x2f) :
            primaryMarker != PrimaryMarkerPlacement::Unexpected;
        const char *markerPlacement = !marker ? "Unexpected" :
            primaryMarker == PrimaryMarkerPlacement::Reflowed ? "Reflowed" : "Original";
        const bool geometry = packet->columns == static_cast<uint32_t>(size.X) &&
            packet->rows == static_cast<uint32_t>(size.Y) && packet->bufferColumns == static_cast<uint32_t>(size.X) &&
            (!alternate || (packet->bufferRows == static_cast<uint32_t>(size.Y) && packet->windowTop == 0 && packet->windowLeft == 0));
        const bool frame = !alternate ||
            (screenTextAt(*packet, 3, 4, L"FRAME " + std::to_wstring(activity.draws), 0x3e) &&
                screenTextAt(*packet, 2, packet->rows - 2, L"EDIT:" + std::wstring(edit), 0x4f) &&
                screenTextAt(*packet, packet->columns - 8, packet->rows - 1, screenCorner, 0x70) &&
                packet->cursorX == edit.size() + 7 && packet->cursorY == packet->rows - 2 &&
                (packet->cursorVisible != 0) == (scenario.exit != ScreenExit::Explicit));
        const bool reconstructed = session.until([&] { return sameScreen(session.state, *packet); }, 2000);
        matched = matched && marker && geometry && frame && reconstructed;
        observations.console = packetJson(*packet);
        observations.comparison = differenceJson(session.state, *packet);
        if (checkpoints.size() > 1) checkpoints += ',';
        checkpoints += "{\"Name\":" + json(utf8(label)) + ",\"NativeActiveMarker\":" + (marker ? "true" : "false") +
            ",\"NativeMarkerPlacement\":" + json(markerPlacement) +
            ",\"NativeGeometryMatches\":" + (geometry ? "true" : "false") + ",\"NativeFrameMatches\":" +
            (frame ? "true" : "false") + ",\"ReconstructedMatches\":" + (reconstructed ? "true" : "false") +
            ",\"NativeCursorVisible\":" + (packet->cursorVisible ? "true" : "false") +
            ",\"ReconstructedCursorVisible\":" + (session.state.visible() ? "true" : "false") +
            ",\"RequestedColumns\":" + std::to_string(size.X) + ",\"RequestedRows\":" + std::to_string(size.Y) +
            ",\"CapturedBytes\":" + std::to_string(session.raw.size()) + ",\"Draws\":" + std::to_string(activity.draws) +
            ",\"WindowEvents\":" + std::to_string(activity.windowEvents) + ",\"Console\":" + observations.console +
            ",\"DecodedState\":" + observations.comparison + '}';
        return activity;
    };
    auto waitFixtureExit = [&] {
        if (!session.until([&] { return WaitForSingleObject(session.descendant.value, 0) == WAIT_OBJECT_0; }))
            throw Mismatch("full-screen child exit deadline");
        DWORD code = 0;
        check(GetExitCodeProcess(session.descendant.value, &code), "full-screen child exit status");
        return code;
    };

    try {
        const auto prompt = startShell(scenario.cmd ? L"CMD_SCREEN" : L"PS_SCREEN", resultDirectory, L"", session, observations);
        const auto atPrompt = [&] {
            const auto row = trimmed(session.state.row(session.state.y()));
            if (row == prompt) return true;
            // Native reflow can shift the restored prompt into leading blank cells.
            return scenario.exit == ScreenExit::Resize && row.ends_with(prompt) &&
                row.find_first_not_of(U' ') == row.size() - prompt.size() &&
                static_cast<size_t>(session.state.x()) == row.size();
        };
        const std::wstring prefix = L"\\\\.\\pipe\\VT7Pty.Screen." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::to_wstring(GetTickCount64());
        const std::wstring fixture = directory() + L"\\FidelityFixture.exe";
        const std::wstring appPipe = prefix + L".app";
        pipe.reset(privateServer(appPipe));
        const auto mode = scenario.exit == ScreenExit::Explicit ? L"SCREEN_HIDDEN" : L"SCREEN_APP";
        session.input(utf8(shellFixtureCommand(scenario.cmd, fixture, mode, appPipe) + L"\r"));
        openShellFixture(session, pipe.value, fixture);
        send('I');
        capture(L"primary", screenInitialSize, false);
        send('A');
        capture(L"alternate", screenInitialSize, true);

        COORD shellSize = screenInitialSize;
        if (scenario.exit == ScreenExit::Explicit) {
            send('M');
            capture(L"explicit-restore", screenInitialSize, false);
        }
        if (scenario.exit == ScreenExit::Resize) {
            send('B');
            session.input("stageX\x7f");
            ScreenActivity last;
            for (uint32_t i = 0; i < 2 * screenResizePairs; ++i) {
                const COORD size = i % 2 == 0 ? screenGrownSize : screenInitialSize;
                const char key = static_cast<char>('1' + i);
                session.input(std::string_view(&key, 1));
                session.pump();
                const auto offset = session.raw.size();
                const auto begin = GetTickCount64();
                resize(size);
                ScreenActivity sample;
                const bool drew = session.until([&] {
                    send('G');
                    transfer(pipe.value, &sample, sizeof(sample), false);
                    return sample.draws > last.draws;
                }, 2000);
                const bool dimensions = sample.columns == static_cast<uint32_t>(size.X) && sample.bufferColumns == sample.columns &&
                    sample.rows == static_cast<uint32_t>(size.Y) && sample.bufferRows == sample.rows &&
                    sample.windowTop == 0 && sample.windowLeft == 0;
                matched = matched && drew && dimensions;
                last = sample;
                if (resizes.size() > 1) resizes += ',';
                resizes += "{\"Columns\":" + std::to_string(size.X) + ",\"Rows\":" + std::to_string(size.Y) +
                    ",\"CapturedBytesBefore\":" + std::to_string(offset) + ",\"Milliseconds\":" +
                    std::to_string(GetTickCount64() - begin) + ",\"ContinuedDrawing\":" + (drew ? "true" : "false") +
                    ",\"NativeGeometryMatches\":" + (dimensions ? "true" : "false") +
                    ",\"Child\":" + screenGeometryJson(sample) + '}';
            }
            send('R');
            uint32_t keys = 7 + 2 * screenResizePairs;
            transfer(pipe.value, &keys, sizeof(keys), true);
            const auto final = capture(L"settled-edit", screenInitialSize, true, L"stage12345678");
            add("FullScreenEditMatches", screenEditMatches(*packet, L"stageX\b12345678"));
            add("ResizeEventsReceived", final.windowEvents != 0);
            send('X');
            char acknowledgement = 0;
            transfer(pipe.value, &acknowledgement, 1, false);
            check(acknowledgement == 'X', "exit/resize acknowledgement");
            observations.add("ExitObservedBeforeResize", WaitForSingleObject(session.descendant.value, 0) == WAIT_OBJECT_0);
            observations.fields += ",\"ExitResizeCapturedBytesBefore\":" + std::to_string(session.raw.size());
            shellSize = screenGrownSize;
            resize(shellSize);
            observations.add("ExitObservedAfterResize", WaitForSingleObject(session.descendant.value, 0) == WAIT_OBJECT_0);
        } else {
            send('Q');
        }

        const auto exitCode = waitFixtureExit();
        observations.fields += ",\"FullScreenExitCode\":" + std::to_string(exitCode);
        add("FullScreenExitMatches", exitCode == screenExitCode);
        const bool returned = session.until(atPrompt);
        add("ReturnedToShellPrompt", returned);
        if (returned) {
            const auto row = trimmed(session.state.row(session.state.y()));
            observations.fields += ",\"ReturnedPromptColumn\":" + std::to_string(row.size() - prompt.size());
        }

        const std::wstring statusPath = resultDirectory + L"\\child-exit-code.txt";
        const std::wstring inspectPipe = prefix + L".inspect";
        pipe.reset(privateServer(inspectPipe));
        const std::wstring saveExit = scenario.cmd ? L"echo %errorlevel%>" + quote(statusPath) + L" & " :
            L"[IO.File]::WriteAllText(" + psLiteral(statusPath) + L",$LASTEXITCODE.ToString()); ";
        session.input(utf8(saveExit + shellFixtureCommand(scenario.cmd, fixture, L"SCREEN_INSPECT", inspectPipe) + L"\r"));
        openShellFixture(session, pipe.value, fixture);
        send('I');
        capture(L"after-exit", shellSize, false);
        const auto reportedExit = fileText(statusPath);
        observations.fields += ",\"ShellReportedChildExit\":" + json(reportedExit);
        add("ShellReportedExitMatches", reportedExit == std::to_string(screenExitCode));
        send('Q');
        add("InspectorExited", waitFixtureExit() == 0);
        pipe.reset();
        session.until(atPrompt);

        const std::wstring continued = resultDirectory + L"\\continued-command.txt";
        const std::wstring beforeEdit = scenario.cmd ? L"echo SCREEN_SHELL_OKX" :
            L"[IO.File]::WriteAllText(" + psLiteral(continued) + L",'SCREEN_SHELL_OKX";
        const std::wstring afterEdit = scenario.cmd ? L">" + quote(continued) + L" & echo SCREEN_SHELL_READY" :
            L"'); Write-Output 'SCREEN_SHELL_READY'";
        const auto lines = session.lineFeeds;
        session.input(utf8(beforeEdit) + "\x7f" + utf8(afterEdit) + "\r");
        const bool ready = session.until([&] {
            return fileText(continued) == "SCREEN_SHELL_OK" && session.lineFeeds > lines &&
                session.state.contains(U"SCREEN_SHELL_READY") && atPrompt();
        });
        add("NextEditedCommandExecuted", fileText(continued) == "SCREEN_SHELL_OK");
        add("ShellRepainted", ready && !session.state.contains(U"SCREEN_ALTERNATE"));
        session.input("exit 0\r");
        session.finish();
        completed = true;
    } catch (...) {
        if (session.descendant.value != INVALID_HANDLE_VALUE) {
            // Preserve a fixture's diagnostic line if it exited before replying.
            try { session.until([&] { return WaitForSingleObject(session.descendant.value, 0) == WAIT_OBJECT_0; }, 250); }
            catch (...) {}
        }
        finishEvidence();
        throw;
    }
    finishEvidence();
    if (!matched) throw Mismatch("full-screen restoration or resize differs");
}

void childCase(const std::wstring &name,const std::wstring &resultDirectory,const std::wstring &program,Observations &observations) {
    const bool negative=name.starts_with(L"NEGATIVE_CHILD");
    const bool cmd=negative || name.starts_with(L"CMD");
    const std::wstring shellName=negative ? L"CMD_CHILD_HANDLED" : name;
    const bool defaultHandler=name.ends_with(L"DEFAULT"), unprocessed=name.ends_with(L"UNPROCESSED");
    const std::wstring mode=defaultHandler ? L"CHILD_DEFAULT" : (unprocessed ? L"CHILD_UNPROCESSED" : L"CHILD_HANDLED");
    Session session;
    Handle pipe;
    bool connected=false;
    try {
        const auto expectedPrompt=startShell(shellName,resultDirectory,program,session,observations);
        auto atPrompt=[&] { return trimmed(session.state.row(session.state.y())) == expectedPrompt; };
        const std::wstring pipeName=L"\\\\.\\pipe\\VT7Pty.Child."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        pipe.reset(privateServer(pipeName));
        const std::wstring fixture=directory()+L"\\FidelityFixture.exe";
        session.input(utf8(shellFixtureCommand(cmd, fixture, mode, pipeName)+L"\r"));
        openShellFixture(session, pipe.value, fixture); connected=true;
        const DWORD peer=GetProcessId(session.descendant.value);
        ChildPacket packet;
        transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        check(packet.stage == 1 && packet.processId == peer && packet.parentId == GetProcessId(session.child.value) &&
            packet.controlCount == 0 && packet.keyCount == 0 &&
            ((packet.inputMode&ENABLE_PROCESSED_INPUT) != 0) == !unprocessed,"child readiness identity and mode");
        saveChild(resultDirectory,L"child-ready.bin",packet);
        observations.fields+=",\"ChildReady\":"+childJson(packet);
        session.input(unprocessed ? "\x03z" : "\x03");
        bool finalPacket=false;
        const bool exited=session.until([&] {
            while (receiveChild(pipe.value,packet)) { finalPacket=packet.stage == 2; }
            return WaitForSingleObject(session.descendant.value,0) == WAIT_OBJECT_0;
        },5000);
        observations.add("ChildExitedAfterCtrlC",exited);
        // A missed interruption remains a workflow failure. Ask only this owned
        // fixture to stop before tearing down its shell and console.
        if (!exited) { char quit='Q'; transfer(pipe.value,&quit,1,true); }
        if (!session.until([&] { return WaitForSingleObject(session.descendant.value,0) == WAIT_OBJECT_0; },5000))
            throw Mismatch("owned native child shutdown deadline");
        while (receiveChild(pipe.value,packet)) { finalPacket=packet.stage == 2; }
        DWORD status=0; check(GetExitCodeProcess(session.descendant.value,&status),"native child exit status");
        saveChild(resultDirectory,L"child-observation.bin",packet);
        observations.fields+=",\"ChildObservation\":"+childJson(packet)+",\"ChildExitCode\":"+std::to_string(status);
        observations.add("ChildFinalPacketReceived",finalPacket);
        ChildPacket oraclePacket=packet;
        DWORD oracleStatus=status;
        if (name == L"NEGATIVE_CHILD_CONTROL") {
            ++oraclePacket.controlCount;
            observations.fields+=",\"InjectedFault\":\"duplicate child control count\"";
        }
        if (name == L"NEGATIVE_CHILD_EXIT") {
            ++oracleStatus;
            observations.fields+=",\"InjectedFault\":\"wrong native child exit status\"";
        }
        const bool control=(defaultHandler || finalPacket) && childInputMatches(oraclePacket,defaultHandler,unprocessed);
        const DWORD expectedStatus=expectedChildExit(defaultHandler);
        const bool statusMatches=oracleStatus == expectedStatus;
        observations.add("ChildControlMatches",control);
        observations.add("ChildExitMatches",statusMatches);
        const bool returned=session.until(atPrompt,5000);
        observations.add("ReturnedToShellPrompt",returned);
        const bool drained=defaultHandler || session.state.contains(U"CHILD_FINAL_DRAIN");
        observations.add("ChildFinalOutputMatches",drained);
        const std::wstring statusPath=resultDirectory+L"\\child-exit-code.txt";
        const std::wstring continuedPath=resultDirectory+L"\\continued-command.txt";
        const std::wstring next=cmd ? L"echo %errorlevel%>"+quote(statusPath)+L" & echo CONTINUE>"+quote(continuedPath) :
            L"[IO.File]::WriteAllText("+psLiteral(statusPath)+L",$LASTEXITCODE.ToString()); [IO.File]::WriteAllText("+psLiteral(continuedPath)+L",'CONTINUE')";
        bool usable=false, reported=false;
        if (returned) {
            session.input(utf8(next+L"\r"));
            usable=session.until([&] { return GetFileAttributesW(continuedPath.c_str()) != INVALID_FILE_ATTRIBUTES && atPrompt(); });
            const auto actual=fileText(statusPath);
            reported=actual == std::to_string(static_cast<int32_t>(expectedStatus)) || actual == std::to_string(expectedStatus);
            observations.fields+=",\"ShellReportedChildExit\":"+json(actual);
        }
        observations.add("ShellReportedExitMatches",reported);
        observations.add("NextCommandExecuted",usable);
        const bool alive=WaitForSingleObject(session.child.value,0) == WAIT_TIMEOUT;
        observations.add("ShellAlive",alive);
        if (alive && returned) { session.input("exit 0\r"); session.finish(); }
        observations.raw=session.raw; observations.cleanup=session.cleanup();
        if (!exited || !control || !statusMatches || !returned || !drained || !reported || !usable || !alive)
            throw Mismatch("active child interruption workflow failed");
    } catch (...) {
        if (connected && session.descendant.value != nullptr && session.descendant.value != INVALID_HANDLE_VALUE &&
            WaitForSingleObject(session.descendant.value,0) == WAIT_TIMEOUT) {
            try { char quit='Q'; transfer(pipe.value,&quit,1,true,1000); WaitForSingleObject(session.descendant.value,5000); } catch (...) {}
        }
        pipe.reset();
        observations.raw=session.raw; observations.cleanup=session.cleanup(); throw;
    }
}
void cookedCase(const std::wstring &name,const std::wstring &resultDirectory,Observations &observations) {
    Session session;
    try {
        const std::wstring pipeName=L"\\\\.\\pipe\\VT7Pty.Cooked."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        Handle pipe(privateServer(pipeName));
        const std::wstring fixture=directory()+L"\\FidelityFixture.exe";
        session.spawn(fixture,quote(fixture)+L" CHILD_COOKED "+quote(pipeName)+L" "+std::to_wstring(GetCurrentProcessId()));
        connect(pipe.value);
        ULONG peer=0; check(GetNamedPipeClientProcessId(pipe.value,&peer) && peer == GetProcessId(session.child.value),"cooked peer PID");
        ChildPacket packet;
        transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        check(packet.stage == 1 && packet.processId == peer && !packet.readReturned &&
            (packet.inputMode&(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT)) ==
            (ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT),"cooked readiness mode");
        session.input("PROBE_TYPED");
        if (!session.until([&] { return session.state.contains(U"COOKED_INPUT>PROBE_TYPED"); }))
            throw std::runtime_error("pending cooked read echo barrier");
        char sample='S'; transfer(pipe.value,&sample,1,true);
        transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        check(!packet.readReturned && !packet.controlCount,"cooked pending read barrier");
        saveChild(resultDirectory,L"child-ready.bin",packet);
        observations.fields+=",\"ChildReady\":"+childJson(packet);
        const bool recordProbe=name == L"READCONSOLE_RECORD_EVENT", ownedCancel=name == L"READCONSOLE_OWNED_CANCEL";
        const bool mechanism=recordProbe || ownedCancel, direct=name == L"READCONSOLE_EVENT" || mechanism;
        observations.fields+=",\"ControlDelivery\":"+json(recordProbe ? "WriteConsoleInputThenGenerateEvent" :
            ownedCancel ? "CancelOwnedReaderThenGenerateEvent" : direct ? "FixtureGenerateConsoleCtrlEvent" : "VTByte03");
        DWORD mechanismReply[3]={};
        if (direct) {
            char command=recordProbe ? 'R' : ownedCancel ? 'K' : 'G'; transfer(pipe.value,&command,1,true);
            if (mechanism) {
                transfer(pipe.value,mechanismReply,sizeof(mechanismReply),false);
                check(mechanismReply[0] <= 1 && (!mechanismReply[0] || mechanismReply[1] == ERROR_SUCCESS) &&
                    (ownedCancel ? mechanismReply[2] == 0 : mechanismReply[2] <= 2),"mechanism reply bounds");
                observations.add("MechanismSucceeded",mechanismReply[0] == 1);
                observations.fields+=",\"MechanismError\":"+std::to_string(mechanismReply[1])+
                    ",\"MechanismRecordsWritten\":"+std::to_string(mechanismReply[2]);
            }
        } else session.input("\x03");
        const bool completed=session.until([&] {
            while (receiveChild(pipe.value,packet)) {}
            return packet.stage == 2;
        },2000);
        if (!completed) {
            transfer(pipe.value,&sample,1,true); transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
        }
        saveChild(resultDirectory,L"child-observation.bin",packet);
        observations.fields+=",\"ChildObservation\":"+childJson(packet);
        const bool signal=packet.controlCount == 1 && packet.controlType == CTRL_C_EVENT;
        const bool canceled=completed && packet.readReturned && packet.textCount == 0 &&
            (!ownedCancel || packet.readError == ERROR_OPERATION_ABORTED);
        observations.add("ControlSignalMatches",signal); observations.add("CookedReadCanceled",canceled);
        if (!completed) {
            session.input("\r");
            if (!session.until([&] { while (receiveChild(pipe.value,packet)) {} return packet.stage == 2; },5000))
                throw Mismatch("cooked boundary Enter recovery deadline");
            observations.fields+=",\"BoundaryEnterRead\":"+childJson(packet);
            saveChild(resultDirectory,L"boundary-enter-read.bin",packet);
        }
        bool reusable=true;
        if (ownedCancel) {
            char next='N'; transfer(pipe.value,&next,1,true);
            transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
            check(packet.stage == 1 && !packet.readReturned,"next cooked read readiness");
            saveChild(resultDirectory,L"child-next-ready.bin",packet);
            observations.fields+=",\"ChildNextReady\":"+childJson(packet);
            session.input("NEXT_PROBE\r");
            const bool nextCompleted=session.until([&] { while (receiveChild(pipe.value,packet)) {} return packet.stage == 2; },5000);
            saveChild(resultDirectory,L"child-next-read.bin",packet);
            observations.fields+=",\"ChildNextRead\":"+childJson(packet);
            reusable=nextCompleted && packet.readReturned && packet.readSucceeded &&
                packet.readError == ERROR_SUCCESS && std::wstring_view(packet.text,packet.textCount) == L"NEXT_PROBE\r\n" &&
                packet.controlCount == 1 && packet.controlType == CTRL_C_EVENT;
            observations.add("NextCookedReadMatches",reusable);
            if (!nextCompleted) {
                // Preserve the failed next read before recovery. A canceled
                // client call alone must not qualify console-side cancellation.
                char sampleNext='S'; transfer(pipe.value,&sampleNext,1,true);
                transfer(pipe.value,&packet,sizeof(packet),false); validateChild(packet);
                saveChild(resultDirectory,L"child-next-pending.bin",packet);
                observations.fields+=",\"ChildNextPending\":"+childJson(packet);
                session.input("\r");
                const bool recovered=session.until([&] { while (receiveChild(pipe.value,packet)) {} return packet.stage == 2; },5000);
                saveChild(resultDirectory,L"next-boundary-enter-read.bin",packet);
                observations.fields+=",\"NextBoundaryEnterRead\":"+childJson(packet);
                observations.add("NextBoundaryEnterRecovered",recovered);
                if (!recovered) throw Mismatch("next cooked read recovery deadline");
            }
        }
        if (!session.until([&] { return WaitForSingleObject(session.child.value,0) == WAIT_OBJECT_0 && session.state.contains(U"CHILD_FINAL_DRAIN"); }))
            throw Mismatch("cooked fixture drain/exit deadline");
        DWORD status=0; check(GetExitCodeProcess(session.child.value,&status),"cooked child exit status");
        observations.fields+=",\"ChildExitCode\":"+std::to_string(status);
        // Drain through EOF while accepting the fixture's deliberate exit 37.
        if (status == 37) session.finish(37);
        observations.raw=session.raw; observations.cleanup=session.cleanup();
        if (!signal || !canceled || !reusable || status != 37 ||
            (mechanism && (!mechanismReply[0] || (recordProbe && mechanismReply[2] != 2)))) throw Mismatch("cooked console read cancellation workflow failed");
    } catch (...) { observations.raw=session.raw; observations.cleanup=session.cleanup(); throw; }
}

void shellCase(const std::wstring &name,const std::wstring &directoryPath,const std::wstring &program,Observations &observations) {
    const bool cmd=name.starts_with(L"CMD"), partial=name.ends_with(L"PARTIAL");
    Session session;
    try {
        const auto expectedPrompt=startShell(name,directoryPath,program,session,observations);
        auto atPrompt=[&] { return trimmed(session.state.row(session.state.y())) == expectedPrompt; };
        const std::wstring canceledPath=directoryPath+L"\\cancelled-command.txt";
        check(GetFileAttributesW(canceledPath.c_str()) == INVALID_FILE_ATTRIBUTES,"cancellation marker preflight");
        if (partial) {
            const std::wstring input=cmd ? L"echo BAD>"+quote(canceledPath) :
                L"[IO.File]::WriteAllText("+psLiteral(canceledPath)+L",'BAD')";
            session.input(utf8(input));
            if (!session.until([&] { return !atPrompt(); })) throw std::runtime_error("partial input not displayed");
        }
        const uint64_t before=session.lineFeeds;
        session.input("\x03");
        const bool fresh=session.until([&] { return session.lineFeeds>before && atPrompt(); },2000);
        observations.add("FreshPromptAfterCtrlC",fresh);
        session.input("\r");
        if (!session.until(atPrompt)) throw std::runtime_error("shell prompt after boundary Enter unavailable");
        // Execute the next command through a separate file barrier so its echoed
        // text cannot falsely count as application output.
        const std::wstring continuedPath=directoryPath+L"\\continued-command.txt";
        const std::wstring next=cmd ? L"echo CONTINUE>"+quote(continuedPath) :
            L"[IO.File]::WriteAllText("+psLiteral(continuedPath)+L",'CONTINUE')";
        session.input(utf8(next+L"\r"));
        const bool usable=session.until([&] { return GetFileAttributesW(continuedPath.c_str()) != INVALID_FILE_ATTRIBUTES && atPrompt(); });
        const bool canceled=GetFileAttributesW(canceledPath.c_str()) == INVALID_FILE_ATTRIBUTES;
        observations.add("CanceledCommandNotExecuted",canceled);
        observations.add("NextCommandExecuted",usable);
        observations.add("ShellAlive",WaitForSingleObject(session.child.value,0) == WAIT_TIMEOUT);
        session.input("exit 0\r"); session.finish();
        if (!fresh || !canceled || !usable) throw Mismatch("shell prompt cancellation workflow failed");
    } catch (...) { observations.raw=session.raw; observations.cleanup=session.cleanup(); throw; }
    observations.raw=session.raw; observations.cleanup=session.cleanup();
}
#include "LifecycleResizeCapture.h"
#include "LifecycleTest.h"
}
int wmain(int argc,wchar_t **argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--lifecycle-cases") {
        puts(lifecycleRecipes().c_str());
        return 0;
    }
    if (argc == 2 && std::wstring_view(argv[1]) == L"--screen-cases") {
        std::string list = "{\"SchemaVersion\":2,\"SnapshotBytes\":" + std::to_string(sizeof(Packet)) +
            ",\"ResizePairs\":" + std::to_string(screenResizePairs) +
            ",\"ViewportResizePairs\":" + std::to_string(viewportResizePairs) + ",\"Cases\":["
            "{\"Name\":\"ALTERNATE\",\"Area\":\"F06\",\"Mode\":\"Control\"},"
            "{\"Name\":\"SCREEN_VIEWPORT\",\"Area\":\"F07\",\"Mode\":\"Viewport\"},"
            "{\"Name\":\"SCREEN_FIXED_ROWS\",\"Area\":\"F07\",\"Mode\":\"FixedRows\"}";
        for (const auto &scenario : screenCases) {
            const char *mode = scenario.exit == ScreenExit::Explicit ? "Explicit" :
                scenario.exit == ScreenExit::Implicit ? "Implicit" : "Resize";
            list += ",{\"Name\":" + json(utf8(scenario.name)) + ",\"Area\":" +
                json(scenario.exit == ScreenExit::Resize ? "F07" : "F06") + ",\"Mode\":" + json(mode) + '}';
        }
        puts((list + "]}").c_str());
        return 0;
    }
    if (argc == 6 && std::wstring_view(argv[1]) == L"--replay-width") {
        try {
            return replayWidthFromFiles(argv);
        } catch (const std::exception &error) {
            fprintf(stderr, "Width replay: %s\n", error.what());
            return 2;
        }
    }
    if (argc == 2 && std::wstring_view(argv[1]) == L"--output-cases") {
        std::string list = "{\"SchemaVersion\":1,\"SnapshotBytes\":" + std::to_string(sizeof(Packet)) + ",\"Cases\":[";
        for (const auto &scenario : outputCases) {
            if (&scenario != outputCases) list += ',';
            list += "{\"Name\":" + json(utf8(scenario.name)) + ",\"Area\":" +
                (scenario.isText() ? "\"F04\"" : "\"F05\"") + ",\"RequestedOutputCP\":" +
                std::to_string(scenario.codePage) + ",\"WidthProbe\":" + (scenario.widthProbe ? "true" : "false") + '}';
        }
        puts((list + "]}").c_str());
        return 0;
    }
    if (argc<3 || argc>4) return 2;
    const std::wstring name=argv[1], outputDirectory=argv[2], program=argc == 4 ? argv[3] : L"";
    const ULONGLONG begin=GetTickCount64();
    Observations observations;
    std::string status="Pass", category="Observed", error;
    int result=0;
    try {
        const std::vector<std::wstring> cases={L"STATE",L"UNICODE",L"ALTERNATE",L"RESIZE",L"INPUT",L"PROCESSED_CTRL_C",L"UNPROCESSED_CTRL_C",L"DRAIN",L"NEGATIVE_CELL",L"NEGATIVE_ATTRIBUTE",L"NEGATIVE_CURSOR",L"NEGATIVE_PACKET",L"NEGATIVE_DUPLICATE",L"NEGATIVE_DRAIN",L"NEGATIVE_TIMEOUT",L"NEGATIVE_ORPHAN",L"CMD_EMPTY",L"CMD_PARTIAL",L"PS_EMPTY",L"PS_PARTIAL",L"PS7_EMPTY",L"PS7_PARTIAL",L"CMD_CHILD_HANDLED",L"CMD_CHILD_DEFAULT",L"CMD_CHILD_UNPROCESSED",L"PS_CHILD_HANDLED",L"PS_CHILD_DEFAULT",L"PS_CHILD_UNPROCESSED",L"PS7_CHILD_HANDLED",L"PS7_CHILD_DEFAULT",L"PS7_CHILD_UNPROCESSED",L"NEGATIVE_CHILD_CONTROL",L"NEGATIVE_CHILD_EXIT",L"READCONSOLE_TRANSPORT",L"READCONSOLE_EVENT",L"READCONSOLE_RECORD_EVENT",L"READCONSOLE_OWNED_CANCEL"};
        const auto *output = findOutputCase(name);
        const auto *screen = findScreenCase(name);
        const auto *lifecycle = findLifecycleCase(name);
        check(output || screen || lifecycle || name == L"SCREEN_VIEWPORT" || name == L"SCREEN_FIXED_ROWS" ||
            std::find(cases.begin(),cases.end(),name) != cases.end(),"unknown case");
        if (lifecycle) lifecycleCase(*lifecycle, outputDirectory, observations, program == L"--resize-trace");
        else if (output) outputCase(*output, outputDirectory, observations);
        else if (screen) screenCase(*screen, outputDirectory, observations);
        else if (name.find(L"_CHILD_") != std::wstring::npos) childCase(name,outputDirectory,program,observations);
        else if (name.starts_with(L"READCONSOLE_")) cookedCase(name,outputDirectory,observations);
        else if (name.starts_with(L"CMD") || name.starts_with(L"PS")) shellCase(name,outputDirectory,program,observations);
        else fixtureCase(name,outputDirectory,observations);
    } catch (const Mismatch &e) { status="Fail"; category="BehaviorMismatch"; error=e.what(); result=1; }
    catch (const std::exception &e) { status="Fail"; category="HarnessOrPrerequisiteError"; error=e.what(); result=2; }
    try {
        writeFile(outputDirectory+L"\\stream.bin",observations.raw);
        const std::string record="{\"SchemaVersion\":1,\"Case\":"+json(utf8(name))+
            ",\"Status\":"+json(status)+",\"Category\":"+json(category)+",\"Error\":"+json(error)+
            ",\"DurationMilliseconds\":"+std::to_string(GetTickCount64()-begin)+
            ",\"OwnedProcessesCleaned\":"+(observations.cleanup ? "true" : "false")+
            ",\"Console\":"+observations.console+",\"DecodedState\":"+observations.comparison+observations.fields+"}";
        writeFile(outputDirectory+L"\\case.json",record);
    } catch (const std::exception &e) { fprintf(stderr,"Cannot preserve result: %s\n",e.what()); return 3; }
    printf("Fidelity %s: %s\n",utf8(name).c_str(),status.c_str());
    return result;
}
