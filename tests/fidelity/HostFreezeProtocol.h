// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include <algorithm>
namespace h3freeze {
constexpr uint32_t magic=0x46524856,version=1,ringCapacity=4096,chunkCapacity=32;
inline uint64_t clock() noexcept { LARGE_INTEGER n={}; QueryPerformanceCounter(&n); return static_cast<uint64_t>(n.QuadPart); }
inline uint64_t frequency() noexcept { LARGE_INTEGER n={}; QueryPerformanceFrequency(&n); return static_cast<uint64_t>(n.QuadPart); }
enum class Command:uint32_t { Bind=1,Snapshot=2,Quiet=3,Selected=4,Coordinated=5,Sham=6,Release=7,Resize=8 };
enum Kind:uint32_t { FreezeBegin=1,FreezeEnd=2,UnfreezeBegin=3,UnfreezeEnd=4,ScrapeBegin=5,ScrapeEnd=6,ResizeBegin=7,ResizeEnd=8,Grant=9,Release=10,Expired=11,DeferredResize=12,PeerClosed=13,ShamGrant=14 };
struct AgentEvent { uint64_t sequence=0,qpc=0,tick=0,generation=0; uint32_t pid=0,thread=0,kind=0,frozen=0; int32_t cols=0,rows=0; };
struct WindowEvent { uint64_t sequence=0,qpc=0,window=0,request=0,lparam=0; int64_t result=0; uint32_t pid=0,thread=0,message=0,wparam=0,phase=0,inSend=0; };
template<typename T> struct Chunk { uint64_t first=0,next=0,total=0,lost=0; uint32_t count=0,reserved=0; T events[chunkCapacity]={}; };
template<typename T> struct Ring {
    uint64_t total=0;
    T events[ringCapacity]={};
    void add(T e) noexcept { e.sequence=++total; events[(total-1)%ringCapacity]=e; }
    void read(uint64_t cursor,Chunk<T> &out) const noexcept {
        out=Chunk<T>{}; out.total=total;
        const uint64_t oldest=total>ringCapacity ? total-ringCapacity : 0;
        out.lost=cursor<oldest ? oldest-cursor : 0;
        cursor=std::max(cursor,oldest); cursor=std::min(cursor,total); out.first=cursor+1;
        while(cursor<total && out.count<chunkCapacity) out.events[out.count++]=events[cursor++%ringCapacity];
        out.next=cursor;
    }
};
struct Request {
    uint32_t signature=magic,schema=version,bytes=sizeof(Request),command=0;
    uint64_t nonce[2]={},sequence=0,deadline=0,cursor=0,controllerCreated=0,agentCreated=0;
    uint32_t controller=0,agent=0,leaseMs=0;
    int32_t cols=0,rows=0;
};
struct Response {
    uint32_t signature=magic,schema=version,bytes=sizeof(Response),status=0;
    uint64_t nonce[2]={},sequence=0,created=0,frequency=0,lease=0,expires=0;
    uint32_t agent=0,controller=0,thread=0,held=0,cachedFrozen=0,mode=0;
    int32_t pendingCols=0,pendingRows=0;
    Chunk<AgentEvent> trace;
};
inline Ring<AgentEvent> agentTrace;
inline uint64_t generation=0;
inline void record(Kind kind,bool frozen,int cols=0,int rows=0) noexcept {
    if(kind==FreezeBegin) ++generation;
    AgentEvent e; e.qpc=clock(); e.tick=GetTickCount64(); e.generation=generation;
    e.pid=GetCurrentProcessId(); e.thread=GetCurrentThreadId(); e.kind=kind; e.frozen=frozen ? 1u : 0u; e.cols=cols; e.rows=rows;
    agentTrace.add(e);
}
}
