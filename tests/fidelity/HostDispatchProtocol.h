// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
namespace hostdispatch {
constexpr uint32_t magic=0x32444856,version=1;
enum class Op:uint32_t { Bind=1,Ping=2,Close=3,SuppressAck=4 };
enum class Result:uint32_t { Hello=0,Accepted=1,Schema=2,Nonce=3,Identity=4,Sequence=5,Expired=6,Operation=7,Closed=8,Received=9 };
struct Identity {
    uint32_t controller=0,controllerThread=0,host=0,thread=0,agent=0,fixture=0;
    uint64_t controllerCreated=0,hostCreated=0,agentCreated=0,fixtureCreated=0,window=0;
};
struct Request {
    uint32_t signature=magic,schema=version,bytes=sizeof(Request);
    Op operation=Op::Ping;
    uint64_t nonce[2]={},sequence=0,deadline=0;
    Identity identity;
};
struct Response {
    uint32_t signature=magic,schema=version,bytes=sizeof(Response);
    Result result=Result::Hello;
    uint64_t nonce[2]={},sequence=0,acceptedSequence=0,hostCreated=0,window=0;
    uint32_t host=0,thread=0,operation=0,validationStage=0,windowOwner=0,windowThread=0;
};
static_assert(sizeof(Identity)==64 && sizeof(Request)==112 && sizeof(Response)==88);
inline bool same(const Identity &a,const Identity &b) { return memcmp(&a,&b,sizeof(a))==0; }
inline Result validate(const Request &r,const Request &bound,uint64_t last,uint64_t now,bool closed) {
    if(r.signature!=magic || r.schema!=version || r.bytes!=sizeof(Request)) return Result::Schema;
    if(r.nonce[0]!=bound.nonce[0] || r.nonce[1]!=bound.nonce[1]) return Result::Nonce;
    if(!same(r.identity,bound.identity)) return Result::Identity;
    if(closed) return Result::Closed;
    if(r.sequence!=last+1 || r.sequence==0) return Result::Sequence;
    if(r.deadline<=now || r.deadline-now>5000) return Result::Expired;
    if(r.operation!=Op::Ping && r.operation!=Op::Close && r.operation!=Op::SuppressAck) return Result::Operation;
    return Result::Accepted;
}
}
