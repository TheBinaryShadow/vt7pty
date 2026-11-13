// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#ifdef VT7PTY_H3_DIAGNOSTIC
#include "HostCancellationTrace.h"
#ifdef VT7PTY_H3_FREEZE
#include "HostFreezeProtocol.h"
#endif
#endif
namespace hostcancel {
#if defined(VT7PTY_H3_FREEZE)
constexpr uint32_t magic=0x46434856,version=3;
constexpr wchar_t probeName[]=L"HostCancellationFreezeProbe.exe",hookName[]=L"HostCancellationFreezeHook.dll";
#elif defined(VT7PTY_H3_DIAGNOSTIC)
constexpr uint32_t magic=0x44434856,version=2;
constexpr wchar_t probeName[]=L"HostCancellationTraceProbe.exe",hookName[]=L"HostCancellationTraceHook.dll";
#else
constexpr uint32_t magic=0x33434856,version=1;
constexpr wchar_t probeName[]=L"HostCancellationProbe.exe",hookName[]=L"HostCancellationHook.dll";
#endif
enum class Op:uint32_t { Bind=1,Ping=2,Close=3,SuppressAck=4,Cancel=5,RestoreControl=6,CancelSuppressAck=7,CancelClose=8,DeniedControl=9
#ifdef VT7PTY_H3_FREEZE
,WindowSnapshot=10
#endif
};
enum class Result:uint32_t { Hello=0,Accepted=1,Schema=2,Nonce=3,Identity=4,Sequence=5,Expired=6,Operation=7,Closed=8,Received=9,AccessDenied=10 };
struct Identity {
    uint32_t controller=0,controllerThread=0,host=0,thread=0,agent=0,fixture=0;
    uint64_t controllerCreated=0,hostCreated=0,agentCreated=0,fixtureCreated=0,window=0;
};
struct Request {
    uint32_t signature=magic,schema=version,bytes=sizeof(Request);
    Op operation=Op::Ping;
    uint64_t nonce[2]={},sequence=0,deadline=0;
    Identity identity;
#ifdef VT7PTY_H3_FREEZE
    uint64_t windowCursor=0;
#endif
};
struct KeyboardEvidence {
    uint32_t captured=0,applied=0,restored=0,equal=0,sent=0,injectedFailure=0,error=0;
    BYTE before[256]={},during[256]={},after[256]={};
};
struct Response {
    uint32_t signature=magic,schema=version,bytes=sizeof(Response);
    Result result=Result::Hello;
    uint64_t nonce[2]={},sequence=0,acceptedSequence=0,hostCreated=0,window=0;
    uint32_t host=0,thread=0,operation=0,validationStage=0,windowOwner=0,windowThread=0;
    KeyboardEvidence keyboard;
    wchar_t desktop[128]={};
#ifdef VT7PTY_H3_DIAGNOSTIC
    h3trace::KeyTrace trace;
#endif
#ifdef VT7PTY_H3_FREEZE
    h3freeze::Chunk<h3freeze::WindowEvent> windowTrace;
#endif
};
static_assert(sizeof(Identity)==64);
#ifdef VT7PTY_H3_FREEZE
static_assert(sizeof(Request)==120);
#else
static_assert(sizeof(Request)==112);
#endif
#ifdef VT7PTY_H3_DIAGNOSTIC
static_assert(sizeof(Response)<16384);
#else
static_assert(sizeof(Response)==1144);
#endif
inline bool same(const Identity &a,const Identity &b) { return memcmp(&a,&b,sizeof(a))==0; }
inline Result validate(const Request &r,const Request &bound,uint64_t last,uint64_t now,bool closed) {
    if(r.signature!=magic || r.schema!=version || r.bytes!=sizeof(Request)) return Result::Schema;
    if(r.nonce[0]!=bound.nonce[0] || r.nonce[1]!=bound.nonce[1]) return Result::Nonce;
    if(!same(r.identity,bound.identity)) return Result::Identity;
    if(closed) return Result::Closed;
    if(r.sequence!=last+1 || r.sequence==0) return Result::Sequence;
    if(r.deadline<=now || r.deadline-now>5000) return Result::Expired;
    if(
#ifdef VT7PTY_H3_FREEZE
        r.operation!=Op::WindowSnapshot &&
#endif
        r.operation!=Op::Ping && r.operation!=Op::Close && r.operation!=Op::SuppressAck && r.operation!=Op::Cancel && r.operation!=Op::RestoreControl && r.operation!=Op::CancelSuppressAck && r.operation!=Op::CancelClose && r.operation!=Op::DeniedControl) return Result::Operation;
    return Result::Accepted;
}
}
