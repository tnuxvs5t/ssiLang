#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace sl {

// ===== Error =====
struct Err : std::runtime_error {
    Err(const std::string& m,int l=0)
        :std::runtime_error("[ln "+std::to_string(l)+"] "+m){}
};
[[noreturn]] inline void die(const std::string& m,int l=0){throw Err(m,l);}

// ===== Tokens =====
enum Tk:uint8_t{
    tNUM,tSTR,tID,
    tTRUE,tFALSE,tNULL,
    tFN,tIF,tELSE,tWHILE,tFOR,tIN,
    tRET,tBRK,tCONT,
    tAND,tOR,tNOT,
    tPLUS,tMINUS,tSTAR,tSLASH,tPCT,
    tEQ,tEQEQ,tNEQ,tLT,tLE,tGT,tGE,
    tLP,tRP,tLB,tRB,tLC,tRC,
    tDOT,tQDOT,tQBR,
    tCOMMA,tCOLON,
    tARROW,tPIPE,tFAT,tDPIPE,
    tAT,tDOLLAR,tAMP,
    tNL,tEOF
};
struct Tok{Tk t;std::string v;int ln;};

// ===== AST =====
struct Node;
using NP=std::shared_ptr<Node>;
struct Node{
    enum K{
        eNUM,eSTR,eBOOL,eNUL,eID,eOUTER,eCTX,ePH,eLIST,eMAP,
        eUN,eBIN,eMEM,eSMEM,eIDX,eSIDX,
        eCALL,eSPLIT,eLAM,eCTXEXPR,eDERIV,eLINK,
        sBIND,sASGN,sFN,sIF,sWHILE,sFOR,
        sRET,sBRK,sCONT,sEXPR,sBLOCK,sCTX
    }k;
    double num=0;
    std::string s;
    Tk op=tEOF;
    std::vector<NP> ch;
    std::vector<std::string> params;
    std::vector<std::pair<std::string,NP>> entries;
    int ln=0;
    static NP mk(K k,int l=0){auto n=std::make_shared<Node>();n->k=k;n->ln=l;return n;}
};

// ===== Forward =====
struct Env;
struct Value;
using VVec=std::vector<Value>;
using VMap=std::vector<std::pair<std::string,Value>>;

// ===== Closure =====
struct Closure{
    std::vector<std::string> params;
    NP body;
    std::shared_ptr<Env> env;
    std::function<Value(std::vector<Value>&,std::shared_ptr<Env>,int)> native;
    bool isNat()const{return !!native;}
};

// ===== Handles =====
struct RefHandle{
    enum Kind{VAR,LREF,MREF}kind;
    std::shared_ptr<Env> env;std::string name;
    std::shared_ptr<VVec> list;int idx=0;
    std::shared_ptr<VMap> map;std::string key;
};
struct TcpHandle{uintptr_t fd=~(uintptr_t)0;bool closed=false;};
struct TcpServerHandle{uintptr_t fd=~(uintptr_t)0;bool closed=false;};
struct PipeHandle{uintptr_t rh=0,wh=0;std::string name;bool closed=false,named=false;};
struct ShmHandle{void*ptr=nullptr;size_t sz=4096;std::string name;uintptr_t hm=0;bool closed=false,owner=false;};
struct ProcHandle{uintptr_t h=0;uint64_t pid=0;bool closed=false,waited=false;int exit_code=0;};
struct NetForm{std::string data;};

// ===== Value =====
struct Value{
    enum Ty:uint8_t{NUM,STR,BOOL,NUL,LIST,MAP,FN,REF,TCP,TCPS,PIPE,SHM,PROC,NET}ty=NUL;
    double n=0;
    std::string s;
    std::shared_ptr<void> o;

    static Value Num(double v){Value r;r.ty=NUM;r.n=v;return r;}
    static Value Str(std::string v){Value r;r.ty=STR;r.s=std::move(v);return r;}
    static Value Bool(bool v){Value r;r.ty=BOOL;r.n=v?1:0;return r;}
    static Value Null(){return{};}
    static Value List(VVec v={}){Value r;r.ty=LIST;r.o=std::make_shared<VVec>(std::move(v));return r;}
    static Value Map(VMap v={}){Value r;r.ty=MAP;r.o=std::make_shared<VMap>(std::move(v));return r;}
    static Value Fn(std::shared_ptr<Closure> c){Value r;r.ty=FN;r.o=std::move(c);return r;}
    static Value MkRef(std::shared_ptr<RefHandle> h){Value r;r.ty=REF;r.o=std::move(h);return r;}
    static Value MkTcp(std::shared_ptr<TcpHandle> h){Value r;r.ty=TCP;r.o=std::move(h);return r;}
    static Value MkTcpS(std::shared_ptr<TcpServerHandle> h){Value r;r.ty=TCPS;r.o=std::move(h);return r;}
    static Value MkPipe(std::shared_ptr<PipeHandle> h){Value r;r.ty=PIPE;r.o=std::move(h);return r;}
    static Value MkShm(std::shared_ptr<ShmHandle> h){Value r;r.ty=SHM;r.o=std::move(h);return r;}
    static Value MkProc(std::shared_ptr<ProcHandle> h){Value r;r.ty=PROC;r.o=std::move(h);return r;}
    static Value MkNet(std::shared_ptr<NetForm> f){Value r;r.ty=NET;r.o=std::move(f);return r;}

    VVec& list(){return*std::static_pointer_cast<VVec>(o);}
    const VVec& list()const{return*std::static_pointer_cast<VVec>(o);}
    VMap& map(){return*std::static_pointer_cast<VMap>(o);}
    const VMap& map()const{return*std::static_pointer_cast<VMap>(o);}
    Closure& fn(){return*std::static_pointer_cast<Closure>(o);}
    RefHandle& ref(){return*std::static_pointer_cast<RefHandle>(o);}
    TcpHandle& tcp(){return*std::static_pointer_cast<TcpHandle>(o);}
    TcpServerHandle& tcps(){return*std::static_pointer_cast<TcpServerHandle>(o);}
    PipeHandle& pipe(){return*std::static_pointer_cast<PipeHandle>(o);}
    ShmHandle& shm(){return*std::static_pointer_cast<ShmHandle>(o);}
    ProcHandle& proc(){return*std::static_pointer_cast<ProcHandle>(o);}
    NetForm& net(){return*std::static_pointer_cast<NetForm>(o);}

    bool IsTrue()const{
        switch(ty){
        case BOOL:return n!=0;case NUL:return false;case NUM:return n!=0;
        case STR:return!s.empty();case LIST:return!list().empty();default:return true;
        }
    }
    const char*TyName()const{
        static const char*nm[]={"number","string","bool","null","list","map",
            "function","link-ref","link-tcp","link-tcp-server","link-pipe","link-shm","link-proc","net-form"};
        return nm[ty];
    }
    std::string ToStr()const;
    bool operator==(const Value&r)const{
        if(ty!=r.ty)return false;
        switch(ty){case NUM:return n==r.n;case STR:return s==r.s;
        case BOOL:return n==r.n;case NUL:return true;default:return o==r.o;}
    }
    bool operator!=(const Value&r)const{return!(*this==r);}
};

// ===== Env =====
struct Env:std::enable_shared_from_this<Env>{
    std::unordered_map<std::string,Value> v;
    std::shared_ptr<Env> par;
    std::string file;
    std::string dir;
    Env(std::shared_ptr<Env> p=nullptr):par(p){
        if(par){file=par->file;dir=par->dir;}
    }
    Value*find(const std::string&k){
        auto it=v.find(k);if(it!=v.end())return&it->second;
        return par?par->find(k):nullptr;
    }
    Value*findOuter(const std::string&k){
        return par?par->find(k):nullptr;
    }
    std::shared_ptr<Env> findEnv(const std::string&k){
        if(v.count(k))return shared_from_this();
        return par?par->findEnv(k):nullptr;
    }
    std::shared_ptr<Env> findOuterEnv(const std::string&k){
        return par?par->findEnv(k):nullptr;
    }
    Value*findCtx(){
        auto it=v.find("__ctx");
        if(it!=v.end())return &it->second;
        return par?par->findCtx():nullptr;
    }
    void set(const std::string&k,Value val){v[k]=std::move(val);}
};

// ===== Value::ToStr =====
inline std::string Value::ToStr()const{
    switch(ty){
    case NUM:{
        if(n==static_cast<int64_t>(n)&&std::abs(n)<1e15)return std::to_string(static_cast<int64_t>(n));
        std::ostringstream os;os<<n;return os.str();
    }
    case STR:return s;
    case BOOL:return n?"true":"false";
    case NUL:return"null";
    case LIST:{
        std::string r="[";auto&l=list();
        for(size_t i=0;i<l.size();i++){if(i)r+=", ";
            r+=(l[i].ty==STR?"\""+l[i].s+"\"":l[i].ToStr());}
        return r+"]";
    }
    case MAP:{
        std::string r="{";auto&m=map();
        for(size_t i=0;i<m.size();i++){if(i)r+=", ";
            r+=m[i].first+": "+(m[i].second.ty==STR?"\""+m[i].second.s+"\"":m[i].second.ToStr());}
        return r+"}";
    }
    case FN:return"<fn>";case REF:return"<link-ref>";case TCP:return"<link-tcp>";
    case TCPS:return"<link-tcp-server>";
    case PIPE:return"<link-pipe>";case SHM:return"<link-shm>";case PROC:return"<link-proc>";
    case NET:return"<net-form>";
    }
    return"?";
}

} // namespace sl
