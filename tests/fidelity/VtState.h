// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fidelity {
struct Cell {
    std::u32string text = U" ";
    uint16_t attributes = 7;
    bool continuation = false;
    bool operator==(const Cell &) const = default;
};

// Deliberately bounded test oracle, independent of the backend emitter/scraper.
// Width rules cover the declared corpus, not arbitrary Unicode terminal policy.
class VtState {
public:
    VtState(int columns, int rows) { resize(columns, rows); }
    void resize(int columns, int rows) {
        if (columns < 1 || columns > 160 || rows < 1 || rows > 60)
            throw std::runtime_error("oracle geometry outside bounds");
        std::vector<Cell> next(static_cast<size_t>(columns * rows));
        for (int y = 0; y < std::min(rows, height_); ++y)
            for (int x = 0; x < std::min(columns, width_); ++x)
                next[static_cast<size_t>(y * columns + x)] = at(x, y);
        width_ = columns; height_ = rows; cells_ = std::move(next);
        x_ = std::clamp(x_, 0, width_ - 1); y_ = std::clamp(y_, 0, height_ - 1);
        wrap_ = false;
    }
    int columns() const { return width_; }
    int rows() const { return height_; }
    int x() const { return x_; }
    int y() const { return y_; }
    bool visible() const { return visible_; }
    const Cell &at(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) throw std::runtime_error("oracle cell outside bounds");
        return cells_.at(static_cast<size_t>(y * width_ + x));
    }
    std::u32string row(int y) const {
        std::u32string result;
        for (int x = 0; x < width_; ++x)
            if (!at(x,y).continuation) result += at(x,y).text;
        return result;
    }
    bool contains(std::u32string_view text) const {
        for (int y = 0; y < height_; ++y)
            if (row(y).find(text) != std::u32string::npos) return true;
        return false;
    }
    std::vector<std::string> takeReplies() {
        std::vector<std::string> result; result.swap(replies_); return result;
    }
    void feed(std::string_view bytes) {
        for (unsigned char byte : bytes) consume(byte);
    }
    void finish() const {
        if (state_ != State::Text || remaining_ != 0)
            throw std::runtime_error("truncated VT/UTF-8 stream");
    }
private:
    enum class State { Text, Escape, Csi, Osc, OscEscape };
    State state_ = State::Text;
    int width_ = 0, height_ = 0, x_ = 0, y_ = 0;
    int foreground_ = 7, background_ = 0;
    bool bold_ = false, inverse_ = false, underline_ = false, conceal_ = false;
    bool visible_ = true, wrap_ = false;
    std::vector<Cell> cells_;
    std::vector<std::string> replies_;
    std::string sequence_;
    uint32_t scalar_ = 0, minimum_ = 0;
    int remaining_ = 0;

    static int ansiToWindows(int value) {
        return ((value & 1) << 2) | (value & 2) | ((value & 4) >> 2) | (value & 8);
    }
    uint16_t attributes() const {
        int fore = foreground_ | (bold_ ? 8 : 0), back = background_;
        if (inverse_) std::swap(fore, back);
        if (conceal_) fore = back;
        return static_cast<uint16_t>(ansiToWindows(fore) |
            (ansiToWindows(back) << 4) | (underline_ ? 0x8000 : 0));
    }
    Cell blank() const { return {U" ", attributes(), false}; }
    void eraseCells(size_t begin,size_t end) {
        if (begin>0 && begin<cells_.size() && cells_[begin].continuation)
            cells_[begin-1]=blank();
        if (end<cells_.size() && cells_[end].continuation)
            cells_[end]=blank();
        std::fill(cells_.begin()+static_cast<ptrdiff_t>(begin),
            cells_.begin()+static_cast<ptrdiff_t>(end),blank());
    }
    void lineFeed() {
        wrap_ = false;
        if (y_ + 1 < height_) { ++y_; return; }
        std::move(cells_.begin() + width_, cells_.end(), cells_.begin());
        std::fill(cells_.end() - width_, cells_.end(), blank());
    }
    void put(char32_t value) {
        if (value >= 0x300 && value <= 0x36f) {
            int index = y_ * width_ + x_ - (wrap_ ? 0 : 1);
            while (index >= 0 && cells_[static_cast<size_t>(index)].continuation) --index;
            if (index < 0) throw std::runtime_error("combining mark without preceding cell");
            cells_[static_cast<size_t>(index)].text += value; return;
        }
        const int width = ((value >= 0x4e00 && value <= 0x9fff) || value == 0x1f642) ? 2 : 1;
        if (wrap_ || x_ + width > width_) { x_ = 0; lineFeed(); }
        if (width > width_) throw std::runtime_error("wide glyph outside oracle geometry");
        const size_t index=static_cast<size_t>(y_ * width_ + x_);
        eraseCells(index,index+static_cast<size_t>(width));
        auto &cell = cells_[index];
        cell = {std::u32string(1, value), attributes(), false};
        if (width == 2)
            cells_[static_cast<size_t>(y_ * width_ + x_ + 1)] = {U"", attributes(), true};
        x_ += width;
        if (x_ == width_) { x_ = width_ - 1; wrap_ = true; }
    }
    std::vector<int> parameters(bool &privateMode) const {
        privateMode = !sequence_.empty() && sequence_[0] == '?';
        std::vector<int> result;
        int number = 0;
        for (size_t i = privateMode ? 1u : 0u; i < sequence_.size(); ++i) {
            const char c = sequence_[i];
            if (c == ';') { result.push_back(number); number = 0; }
            else if (c >= '0' && c <= '9') {
                if (number > 1000) throw std::runtime_error("VT parameter outside bounds");
                number = number * 10 + (c - '0');
            } else throw std::runtime_error("unsupported VT parameter");
            if (result.size() > 32) throw std::runtime_error("too many VT parameters");
        }
        result.push_back(number); return result;
    }
    void sgr(const std::vector<int> &p) {
        for (const int value : p) {
            if (value == 0) {
                foreground_ = 7; background_ = 0;
                bold_ = inverse_ = underline_ = conceal_ = false;
            } else if (value == 1) bold_ = true;
            else if (value == 22) bold_ = false;
            else if (value == 4) underline_ = true;
            else if (value == 24) underline_ = false;
            else if (value == 7) inverse_ = true;
            else if (value == 27) inverse_ = false;
            else if (value == 8) conceal_ = true;
            else if (value == 28) conceal_ = false;
            else if (value == 39) foreground_ = 7;
            else if (value == 49) background_ = 0;
            else if (value >= 30 && value <= 37) foreground_ = value - 30;
            else if (value >= 40 && value <= 47) background_ = value - 40;
            else if (value >= 90 && value <= 97) foreground_ = value - 90 + 8;
            else if (value >= 100 && value <= 107) background_ = value - 100 + 8;
            else throw std::runtime_error("unsupported SGR");
        }
    }
    void csi(char final) {
        bool privateMode = false; const auto p = parameters(privateMode);
        const int n = std::max(p[0], 1);
        if (privateMode) {
            if ((final != 'h' && final != 'l') || p.size() != 1)
                throw std::runtime_error("unsupported private mode");
            if (p[0] == 25) visible_ = final == 'h';
            else if (p[0] != 1000 && p[0] != 1002 && p[0] != 1003 &&
                     p[0] != 1005 && p[0] != 1006 && p[0] != 1015)
                throw std::runtime_error("unknown private mode");
            return;
        }
        if (final == 'm') { sgr(p); return; }
        if (final == 'n' && p.size() == 1 && p[0] == 6) {
            replies_.push_back("\x1b[" + std::to_string(y_ + 1) + ";" + std::to_string(x_ + 1) + "R");
            return;
        }
        wrap_ = false;
        if (final == 'H' || final == 'f') {
            if (p.size() > 2) throw std::runtime_error("invalid CUP");
            y_ = std::clamp(n - 1, 0, height_ - 1);
            x_ = std::clamp((p.size() > 1 ? std::max(p[1],1) : 1) - 1, 0, width_ - 1);
        } else if (p.size() != 1) throw std::runtime_error("unexpected VT parameter count");
        else if (final == 'A') y_ = std::max(0, y_ - n);
        else if (final == 'B') y_ = std::min(height_ - 1, y_ + n);
        else if (final == 'C') x_ = std::min(width_ - 1, x_ + n);
        else if (final == 'D') x_ = std::max(0, x_ - n);
        else if (final == 'G') x_ = std::clamp(n - 1, 0, width_ - 1);
        else if (final == 'd') y_ = std::clamp(n - 1, 0, height_ - 1);
        else if (final == 'J' && p[0] >= 0 && p[0] <= 2) {
            const size_t cursor = static_cast<size_t>(y_ * width_ + x_);
            const size_t begin = p[0] == 0 ? cursor : 0;
            const size_t end = p[0] == 1 ? cursor + 1 : cells_.size();
            eraseCells(begin,end);
        } else if (final == 'K' && p[0] >= 0 && p[0] <= 2) {
            const int start = p[0] == 0 ? x_ : 0, end = p[0] == 1 ? x_ + 1 : width_;
            eraseCells(static_cast<size_t>(y_ * width_ + start),
                static_cast<size_t>(y_ * width_ + end));
        } else throw std::runtime_error("unsupported CSI");
    }
    void finishOsc() {
        if (!sequence_.starts_with("0;") && !sequence_.starts_with("2;"))
            throw std::runtime_error("unsupported OSC");
        state_ = State::Text; sequence_.clear();
    }
    void consume(unsigned char byte) {
        if (state_ == State::Escape) {
            sequence_.clear();
            if (byte == '[') state_ = State::Csi;
            else if (byte == ']') state_ = State::Osc;
            else throw std::runtime_error("unsupported escape");
            return;
        }
        if (state_ == State::Csi) {
            if (byte >= 0x40 && byte <= 0x7e) { csi(static_cast<char>(byte)); state_ = State::Text; }
            else {
                if (sequence_.size() >= 128) throw std::runtime_error("CSI exceeds bound");
                sequence_.push_back(static_cast<char>(byte));
            }
            return;
        }
        if (state_ == State::OscEscape) {
            if (byte != '\\') throw std::runtime_error("invalid OSC terminator");
            finishOsc(); return;
        }
        if (state_ == State::Osc) {
            if (byte == 7) finishOsc();
            else if (byte == 0x1b) state_ = State::OscEscape;
            else {
                if (sequence_.size() >= 1024) throw std::runtime_error("OSC exceeds bound");
                sequence_.push_back(static_cast<char>(byte));
            }
            return;
        }
        if (remaining_ != 0) {
            if ((byte & 0xc0) != 0x80) throw std::runtime_error("invalid UTF-8 continuation");
            scalar_ = (scalar_ << 6) | (byte & 0x3f);
            if (--remaining_ == 0) {
                if (scalar_ < minimum_ || scalar_ > 0x10ffff || (scalar_ >= 0xd800 && scalar_ <= 0xdfff))
                    throw std::runtime_error("invalid UTF-8 scalar");
                put(static_cast<char32_t>(scalar_));
            }
        } else if (byte >= 0x80) {
            if (byte >= 0xc2 && byte <= 0xdf) { scalar_ = byte & 0x1f; remaining_ = 1; minimum_ = 0x80; }
            else if (byte >= 0xe0 && byte <= 0xef) { scalar_ = byte & 0x0f; remaining_ = 2; minimum_ = 0x800; }
            else if (byte >= 0xf0 && byte <= 0xf4) { scalar_ = byte & 7; remaining_ = 3; minimum_ = 0x10000; }
            else throw std::runtime_error("invalid UTF-8 lead");
        } else if (byte == 0x1b) state_ = State::Escape;
        else if (byte == '\r') { x_ = 0; wrap_ = false; }
        else if (byte == '\n') lineFeed();
        else if (byte == '\b') { x_ = std::max(0,x_ - 1); wrap_ = false; }
        else if (byte == '\t') { x_ = std::min(width_ - 1, ((x_ / 8) + 1) * 8); wrap_ = false; }
        else if (byte >= 0x20 && byte != 0x7f) put(static_cast<char32_t>(byte));
        else if (byte != 0 && byte != 7 && byte != 0x7f) throw std::runtime_error("unsupported control byte");
    }
};
} // namespace fidelity
