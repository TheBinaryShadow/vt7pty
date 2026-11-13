// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include "HostFreezeProtocol.h"
#include <string>
namespace h3freeze {
inline std::string number(uint64_t n) { return "\""+std::to_string(n)+"\""; }
inline std::string chunkStart(uint64_t total,uint64_t next,uint64_t lost) {
    return "{\"Total\":"+number(total)+",\"NextCursor\":"+number(next)+",\"Lost\":"+number(lost)+",\"Events\":[";
}
inline std::string windowJson(const Chunk<WindowEvent> &t) {
    auto s=chunkStart(t.total,t.next,t.lost);
    for(uint32_t i=0;i<t.count;++i) {
        if(i) s+=','; const auto &e=t.events[i];
        s+="{\"Sequence\":"+number(e.sequence)+",\"Qpc\":"+number(e.qpc)+",\"Window\":"+number(e.window)+",\"Request\":"+number(e.request)+
            ",\"Pid\":"+std::to_string(e.pid)+",\"Thread\":"+std::to_string(e.thread)+",\"Message\":"+std::to_string(e.message)+
            ",\"WParam\":"+std::to_string(e.wparam)+",\"LParam\":"+number(e.lparam)+",\"Result\":\""+std::to_string(e.result)+
            "\",\"Phase\":"+std::to_string(e.phase)+",\"InSend\":"+std::to_string(e.inSend)+"}";
    }
    return s+"]}";
}
inline std::string agentJson(const Response &r) {
    auto s="{\"Agent\":"+std::to_string(r.agent)+",\"Controller\":"+std::to_string(r.controller)+",\"Created\":"+number(r.created)+
        ",\"Thread\":"+std::to_string(r.thread)+",\"Frequency\":"+number(r.frequency)+",\"Sequence\":"+number(r.sequence)+
        ",\"Status\":"+std::to_string(r.status)+",\"Held\":"+std::to_string(r.held)+",\"CachedFrozen\":"+std::to_string(r.cachedFrozen)+
        ",\"Mode\":"+std::to_string(r.mode)+",\"Lease\":"+number(r.lease)+",\"Expires\":"+number(r.expires)+
        ",\"PendingCols\":"+std::to_string(r.pendingCols)+",\"PendingRows\":"+std::to_string(r.pendingRows)+",\"Trace\":"+chunkStart(r.trace.total,r.trace.next,r.trace.lost);
    for(uint32_t i=0;i<r.trace.count;++i) {
        if(i) s+=','; const auto &e=r.trace.events[i];
        s+="{\"Sequence\":"+number(e.sequence)+",\"Qpc\":"+number(e.qpc)+",\"Tick\":"+number(e.tick)+",\"Generation\":"+number(e.generation)+
            ",\"Pid\":"+std::to_string(e.pid)+",\"Thread\":"+std::to_string(e.thread)+",\"Kind\":"+std::to_string(e.kind)+
            ",\"Frozen\":"+std::to_string(e.frozen)+",\"Cols\":"+std::to_string(e.cols)+",\"Rows\":"+std::to_string(e.rows)+"}";
    }
    return s+"]}}";
}
}
