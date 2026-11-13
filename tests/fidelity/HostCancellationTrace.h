// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
namespace h3trace {
constexpr uint32_t capacity=16,timelineCapacity=64;
enum Stage:uint32_t { Captured=1,Applied=2,BeforeKey=3,KeyEntry=4,KeyReturn=5,AfterKey=6,Restored=7,InjectedFailure=8 };
enum TimelineKind:uint32_t { FixtureReady=1,ReadEntry=2,ReadReturn=3,HandlerEntry=4,HandlerExit=5,ShellStarted=6 };
inline uint64_t qpc() noexcept { LARGE_INTEGER n={}; return QueryPerformanceCounter(&n) ? static_cast<uint64_t>(n.QuadPart) : 0; }
inline uint64_t frequency() noexcept { LARGE_INTEGER n={}; return QueryPerformanceFrequency(&n) ? static_cast<uint64_t>(n.QuadPart) : 0; }
struct KeySample {
    uint32_t stage=0,pid=0,thread=0,message=0,owner=0,windowThread=0,inSend=0,keyStateOkay=0,error=0;
    uint64_t qpc=0,tick=0,window=0,wparam=0,lparam=0,layout=0;
    int64_t result=0;
    int32_t ctrl=0,leftCtrl=0,rightCtrl=0,shift=0,alt=0;
    BYTE keys[256]={};
};
struct KeyTrace {
    uint64_t sequence=0,frequency=0;
    uint32_t count=0,overflow=0;
    KeySample samples[capacity]={};
};
inline KeyTrace keyTrace;
inline bool active=false;
inline HWND expectedWindow=nullptr;
inline void begin(uint64_t sequence,HWND window) noexcept {
    keyTrace=KeyTrace{}; keyTrace.sequence=sequence; keyTrace.frequency=frequency(); expectedWindow=window; active=true;
}
inline void sample(Stage stage,HWND window=nullptr,UINT message=0,WPARAM wp=0,LPARAM lp=0,LRESULT result=0) noexcept {
    if(!active) return;
    if(keyTrace.count==capacity) { keyTrace.overflow=1; return; }
    auto &e=keyTrace.samples[keyTrace.count++];
    e.stage=stage; e.qpc=qpc(); e.tick=GetTickCount64(); e.pid=GetCurrentProcessId(); e.thread=GetCurrentThreadId();
    e.window=reinterpret_cast<uintptr_t>(window); e.message=message; e.wparam=wp; e.lparam=static_cast<uint64_t>(lp);
    e.result=result; e.layout=reinterpret_cast<uintptr_t>(GetKeyboardLayout(0)); e.inSend=InSendMessageEx(nullptr);
    DWORD owner=0;
    e.windowThread=window ? GetWindowThreadProcessId(window,&owner) : 0; e.owner=owner;
    e.keyStateOkay=GetKeyboardState(e.keys)!=FALSE; if(!e.keyStateOkay) e.error=GetLastError();
    e.ctrl=GetKeyState(VK_CONTROL); e.leftCtrl=GetKeyState(VK_LCONTROL); e.rightCtrl=GetKeyState(VK_RCONTROL);
    e.shift=GetKeyState(VK_SHIFT); e.alt=GetKeyState(VK_MENU);
}
struct TimelineEvent {
    uint64_t qpc=0,tick=0;
    uint32_t kind=0,thread=0,index=0,value=0,detail=0,error=0,sequence=0;
};
struct Slot { TimelineEvent event; volatile LONG published=0; };
struct Timeline {
    volatile LONG reserved=0;
    Slot slots[timelineCapacity]={};
    void append(TimelineKind kind,uint32_t index=0,uint32_t value=0,uint32_t detail=0,uint32_t error=0) noexcept {
        const LONG n=InterlockedIncrement(&reserved)-1;
        if(n<0 || n>=static_cast<LONG>(timelineCapacity)) return;
        auto &s=slots[n]; auto &e=s.event;
        e.qpc=qpc(); e.tick=GetTickCount64(); e.kind=kind; e.thread=GetCurrentThreadId();
        e.index=index; e.value=value; e.detail=detail; e.error=error; e.sequence=static_cast<uint32_t>(n+1);
        InterlockedExchange(&s.published,n+1); // Publish immutable storage after all fields.
    }
};
struct Observation {
    uint64_t qpc=0,tick=0,frequency=0,beginQpc=0;
    uint32_t eventCount=0,overflow=0,inputRecords=0,inputError=0,selectionFlags=0,selectionError=0;
    uint32_t readEntered=0,customHandlerRegistered=0,ignoreAttributeObserved=0,keyScanCount=0;
    uint32_t keyScanCodes[16]={};
    TimelineEvent events[timelineCapacity]={};
};
inline Timeline fixtureTimeline;
inline volatile LONG fixtureReadIndex=0;
inline void observe(Timeline &timeline,Observation &o) noexcept {
    if(!o.beginQpc) o.beginQpc=qpc(); o.tick=GetTickCount64(); o.frequency=frequency();
    const LONG reserved=InterlockedCompareExchange(&timeline.reserved,0,0);
    o.overflow=reserved>static_cast<LONG>(timelineCapacity) ? 1u : 0u;
    for(LONG i=0;i<reserved && i<static_cast<LONG>(timelineCapacity);++i) {
        if(InterlockedCompareExchange(&timeline.slots[i].published,0,0)==i+1)
            o.events[o.eventCount++]=timeline.slots[i].event;
    }
    o.qpc=qpc();
}
}
