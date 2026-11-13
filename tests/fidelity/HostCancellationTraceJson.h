// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "HostCancellationTrace.h"
#include <string>
namespace h3trace {
inline std::string hex(const BYTE *p) {
    constexpr char digits[]="0123456789abcdef"; std::string s="\""; s.reserve(514);
    for(unsigned int i=0;i<256;++i) { s+=digits[p[i]>>4]; s+=digits[p[i]&15]; }
    return s+"\"";
}
inline std::string u64(uint64_t n) { return "\""+std::to_string(n)+"\""; }
inline std::string keyJson(const KeyTrace &t) {
    std::string s="{\"Sequence\":"+u64(t.sequence)+",\"Frequency\":"+u64(t.frequency)+",\"Overflow\":"+std::to_string(t.overflow)+",\"Events\":[";
    for(uint32_t i=0;i<t.count && i<capacity;++i) {
        if(i) s+=','; const auto &e=t.samples[i];
        s+="{\"Stage\":"+std::to_string(e.stage)+",\"Qpc\":"+u64(e.qpc)+",\"Tick\":"+u64(e.tick)+
            ",\"Pid\":"+std::to_string(e.pid)+",\"Thread\":"+std::to_string(e.thread)+",\"Message\":"+std::to_string(e.message)+
            ",\"Window\":"+u64(e.window)+",\"WindowOwner\":"+std::to_string(e.owner)+",\"WindowThread\":"+std::to_string(e.windowThread)+
            ",\"WParam\":"+u64(e.wparam)+",\"LParam\":"+u64(e.lparam)+",\"Result\":\""+std::to_string(e.result)+"\""+
            ",\"Layout\":"+u64(e.layout)+",\"InSend\":"+std::to_string(e.inSend)+",\"KeyStateOkay\":"+std::to_string(e.keyStateOkay)+
            ",\"Error\":"+std::to_string(e.error)+",\"Ctrl\":"+std::to_string(e.ctrl)+",\"LeftCtrl\":"+std::to_string(e.leftCtrl)+
            ",\"RightCtrl\":"+std::to_string(e.rightCtrl)+",\"Shift\":"+std::to_string(e.shift)+",\"Alt\":"+std::to_string(e.alt)+
            ",\"Keys\":"+hex(e.keys)+"}";
    }
    return s+"]}";
}
inline std::string observationJson(const Observation &o) {
    std::string s="{\"BeginQpc\":"+u64(o.beginQpc)+",\"Qpc\":"+u64(o.qpc)+",\"Tick\":"+u64(o.tick)+",\"Frequency\":"+u64(o.frequency)+
        ",\"AtomicSnapshot\":false,\"ReadEntered\":"+std::to_string(o.readEntered)+",\"InputRecords\":"+std::to_string(o.inputRecords)+",\"InputError\":"+std::to_string(o.inputError)+
        ",\"SelectionFlags\":"+std::to_string(o.selectionFlags)+",\"SelectionError\":"+std::to_string(o.selectionError)+
        ",\"CustomHandlerRegistered\":"+std::to_string(o.customHandlerRegistered)+",\"IgnoreAttributeObserved\":"+std::to_string(o.ignoreAttributeObserved)+
        ",\"Overflow\":"+std::to_string(o.overflow)+",\"Events\":[";
    for(uint32_t i=0;i<o.eventCount && i<timelineCapacity;++i) {
        if(i) s+=','; const auto &e=o.events[i];
        s+="{\"Sequence\":"+std::to_string(e.sequence)+",\"Kind\":"+std::to_string(e.kind)+",\"Qpc\":"+u64(e.qpc)+",\"Tick\":"+u64(e.tick)+
            ",\"Thread\":"+std::to_string(e.thread)+",\"ReadIndex\":"+std::to_string(e.index)+",\"Value\":"+std::to_string(e.value)+
            ",\"Detail\":"+std::to_string(e.detail)+",\"Error\":"+std::to_string(e.error)+"}";
    }
    s+="],\"KeyScanCodes\":[";
    for(uint32_t i=0;i<o.keyScanCount && i<16;++i) { if(i) s+=','; s+=std::to_string(o.keyScanCodes[i]); }
    return s+"]}";
}
}
