#pragma once
#include "parser.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <limits>
#include <thread>
#include <chrono>
#include <unordered_set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace sl {

namespace fs = std::filesystem;

// ===== Control flow signals =====
struct RetSig{Value val;};
struct BrkSig{};
struct ContSig{};

// ===== Serialization =====
inline std::string ser_inner(const Value&v){
    std::ostringstream o;
    switch(v.ty){
    case Value::NUM:o<<"N"<<v.n<<"\n";break;
    case Value::STR:o<<"S"<<v.s.size()<<":"<<v.s;break;
    case Value::BOOL:o<<"B"<<(v.n?"1":"0")<<"\n";break;
    case Value::NUL:o<<"Z\n";break;
    case Value::LIST:{auto&l=v.list();o<<"L"<<l.size()<<"\n";for(auto&e:l)o<<ser_inner(e);break;}
    case Value::MAP:{auto&m=v.map();o<<"M"<<m.size()<<"\n";
        for(auto&[k,val]:m){o<<"S"<<k.size()<<":"<<k;o<<ser_inner(val);}break;}
    default:die("Cannot serialize "+std::string(v.TyName())+"; only number/string/bool/null/list/map supported");
    }
    return o.str();
}
inline std::string ser(const Value&v){
    return "V1\n"+ser_inner(v);
}

inline Value deser_inner(const std::string&d,size_t&pos){
    if(pos>=d.size())return Value::Null();
    char tag=d[pos++];
    switch(tag){
    case'N':{size_t e=d.find('\n',pos);double n=std::stod(d.substr(pos,e-pos));pos=e+1;return Value::Num(n);}
    case'S':{size_t c=d.find(':',pos);int len=std::stoi(d.substr(pos,c-pos));pos=c+1;
        std::string s=d.substr(pos,len);pos+=len;return Value::Str(s);}
    case'B':{bool b=d[pos]=='1';pos+=2;return Value::Bool(b);}
    case'Z':pos++;return Value::Null();
    case'L':{size_t e=d.find('\n',pos);int cnt=std::stoi(d.substr(pos,e-pos));pos=e+1;
        VVec l;for(int i=0;i<cnt;i++)l.push_back(deser_inner(d,pos));return Value::List(std::move(l));}
    case'M':{size_t e=d.find('\n',pos);int cnt=std::stoi(d.substr(pos,e-pos));pos=e+1;
        VMap m;for(int i=0;i<cnt;i++){auto k=deser_inner(d,pos);
            if(k.ty!=Value::STR)die("Corrupted data: map key must be string");
            auto val=deser_inner(d,pos);m.push_back({k.s,val});}
        return Value::Map(std::move(m));}
    default:return Value::Null();
    }
}

inline Value deser(const std::string&d,size_t&pos){
    if(pos>=d.size())return Value::Null();
    // Check for version header
    if(d[pos]=='V'){
        size_t nl=d.find('\n',pos);
        if(nl!=std::string::npos){pos=nl+1;}
    }
    return deser_inner(d,pos);
}

// ===== Platform net helpers =====
namespace pnet{
    inline void writeU32LE(char*out,uint32_t v){
        out[0]=(char)(v&0xFF);
        out[1]=(char)((v>>8)&0xFF);
        out[2]=(char)((v>>16)&0xFF);
        out[3]=(char)((v>>24)&0xFF);
    }
    inline uint32_t readU32LE(const char*in){
        return ((uint32_t)(unsigned char)in[0]) |
               (((uint32_t)(unsigned char)in[1])<<8) |
               (((uint32_t)(unsigned char)in[2])<<16) |
               (((uint32_t)(unsigned char)in[3])<<24);
    }
    inline std::string sysErr(){
#ifdef _WIN32
        return " (err="+std::to_string(WSAGetLastError())+")";
#else
        return " ("+std::string(strerror(errno))+")";
#endif
    }

    inline void sendAll(uintptr_t fd,const char*buf,int len){
        int sent=0;
        while(sent<len){
#ifdef _WIN32
            int n=::send((SOCKET)fd,buf+sent,len-sent,0);
#else
            int n=::send((int)fd,buf+sent,len-sent,0);
#endif
            if(n<=0)die("send failed: wrote "+std::to_string(sent)+"/"+std::to_string(len)+sysErr());
            sent+=n;
        }
    }
    inline void recvAll(uintptr_t fd,char*buf,int len){
        int got=0;
        while(got<len){
#ifdef _WIN32
            int n=::recv((SOCKET)fd,buf+got,len-got,0);
#else
            int n=::recv((int)fd,buf+got,len-got,0);
#endif
            if(n<=0)die("recv failed: got "+std::to_string(got)+"/"+std::to_string(len)+sysErr());
            got+=n;
        }
    }
    inline void sendMsg(uintptr_t fd,const std::string&data){
        uint32_t len=(uint32_t)data.size();
        char hdr[4];
        writeU32LE(hdr,len);
        sendAll(fd,hdr,4);
        sendAll(fd,data.c_str(),len);
    }
    inline std::string recvMsg(uintptr_t fd){
        char hdr[4];
        recvAll(fd,hdr,4);
        uint32_t len=readU32LE(hdr);
        std::string buf(len,0);
        recvAll(fd,&buf[0],len);
        return buf;
    }
    // Pipe-specific I/O using write/read (not send/recv which are socket-only)
    inline void pipeWriteAll(uintptr_t fd,const char*buf,int len){
        int sent=0;
        while(sent<len){
#ifdef _WIN32
            DWORD n=0;
            if(!WriteFile((HANDLE)fd,buf+sent,len-sent,&n,NULL)||n==0)die("pipe write failed"+sysErr());
            sent+=(int)n;
#else
            int n=::write((int)fd,buf+sent,len-sent);
            if(n<=0)die("pipe write failed"+sysErr());
            sent+=n;
#endif
        }
    }
    inline void pipeReadAll(uintptr_t fd,char*buf,int len){
        int got=0;
        while(got<len){
#ifdef _WIN32
            DWORD n=0;
            if(!ReadFile((HANDLE)fd,buf+got,len-got,&n,NULL)||n==0)die("pipe read failed"+sysErr());
            got+=(int)n;
#else
            int n=::read((int)fd,buf+got,len-got);
            if(n<=0)die("pipe read failed"+sysErr());
            got+=n;
#endif
        }
    }
    inline void pipeSendMsg(uintptr_t fd,const std::string&data){
        uint32_t len=(uint32_t)data.size();
        char hdr[4];
        writeU32LE(hdr,len);
        pipeWriteAll(fd,hdr,4);
        pipeWriteAll(fd,data.c_str(),len);
    }
    inline std::string pipeRecvMsg(uintptr_t fd){
        char hdr[4];
        pipeReadAll(fd,hdr,4);
        uint32_t len=readU32LE(hdr);
        std::string buf(len,0);
        pipeReadAll(fd,&buf[0],len);
        return buf;
    }

    // ===== SHM Spinlock =====
    inline void shmLock(void*p){
        volatile uint32_t*lk=(volatile uint32_t*)p;
#ifdef _WIN32
        while(InterlockedExchange((LONG volatile*)lk,1)){}
#else
        while(__sync_lock_test_and_set(lk,1)){}
#endif
    }
    inline void shmUnlock(void*p){
        volatile uint32_t*lk=(volatile uint32_t*)p;
#ifdef _WIN32
        InterlockedExchange((LONG volatile*)lk,0);
#else
        __sync_lock_release(lk);
#endif
    }
}

// ===== JSON helpers =====
namespace json{
    inline std::string encode(const Value&v){
        switch(v.ty){
        case Value::NUM:{
            if(v.n==static_cast<int64_t>(v.n)&&std::abs(v.n)<1e15)
                return std::to_string(static_cast<int64_t>(v.n));
            std::ostringstream os;os<<v.n;return os.str();
        }
        case Value::STR:{
            std::string r="\"";
            for(char c:v.s){
                switch(c){
                case'"':r+="\\\"";break;
                case'\\':r+="\\\\";break;
                case'\n':r+="\\n";break;
                case'\t':r+="\\t";break;
                case'\r':r+="\\r";break;
                case'\b':r+="\\b";break;
                case'\f':r+="\\f";break;
                default:
                    if((unsigned char)c<0x20){
                        char buf[8];snprintf(buf,sizeof(buf),"\\u%04x",(unsigned char)c);
                        r+=buf;
                    }else r+=c;
                }
            }
            return r+"\"";
        }
        case Value::BOOL:return v.n?"true":"false";
        case Value::NUL:return"null";
        case Value::LIST:{
            std::string r="[";auto&l=v.list();
            for(size_t i=0;i<l.size();i++){if(i)r+=",";r+=encode(l[i]);}
            return r+"]";
        }
        case Value::MAP:{
            std::string r="{";auto&m=v.map();
            for(size_t i=0;i<m.size();i++){if(i)r+=",";
                r+="\""+m[i].first+"\":"+encode(m[i].second);}
            return r+"}";
        }
        default:die("Cannot JSON-encode "+std::string(v.TyName()));
        }
        return"";
    }

    inline void skipWS(const std::string&s,size_t&p){
        while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))p++;
    }

    inline Value parse(const std::string&s,size_t&p);

    inline std::string parseStr(const std::string&s,size_t&p){
        p++; // skip opening "
        std::string r;
        while(p<s.size()&&s[p]!='"'){
            if(s[p]=='\\'){
                p++;if(p>=s.size())die("Unterminated JSON string");
                switch(s[p]){
                case'"':r+='"';break;case'\\':r+='\\';break;
                case'/':r+='/';break;case'n':r+='\n';break;
                case't':r+='\t';break;case'r':r+='\r';break;
                case'b':r+='\b';break;case'f':r+='\f';break;
                case'u':{
                    if(p+4>=s.size())die("Bad \\u escape in JSON");
                    unsigned cp=0;
                    for(int i=0;i<4;i++){
                        p++;char c=s[p];
                        cp<<=4;
                        if(c>='0'&&c<='9')cp+=c-'0';
                        else if(c>='a'&&c<='f')cp+=c-'a'+10;
                        else if(c>='A'&&c<='F')cp+=c-'A'+10;
                        else die("Bad hex in \\u escape");
                    }
                    if(cp<0x80)r+=(char)cp;
                    else if(cp<0x800){r+=(char)(0xC0|(cp>>6));r+=(char)(0x80|(cp&0x3F));}
                    else{r+=(char)(0xE0|(cp>>12));r+=(char)(0x80|((cp>>6)&0x3F));r+=(char)(0x80|(cp&0x3F));}
                    break;
                }
                default:r+=s[p];
                }
            }else r+=s[p];
            p++;
        }
        if(p>=s.size())die("Unterminated JSON string");
        p++; // skip closing "
        return r;
    }

    inline Value parse(const std::string&s,size_t&p){
        skipWS(s,p);
        if(p>=s.size())die("Unexpected end of JSON");
        char c=s[p];
        if(c=='"'){
            return Value::Str(parseStr(s,p));
        }
        if(c=='{'){ // object
            p++;skipWS(s,p);
            VMap m;
            if(p<s.size()&&s[p]=='}'){p++;return Value::Map(std::move(m));}
            while(true){
                skipWS(s,p);
                if(p>=s.size()||s[p]!='"')die("Expected string key in JSON object");
                std::string key=parseStr(s,p);
                skipWS(s,p);
                if(p>=s.size()||s[p]!=':')die("Expected ':' in JSON object");
                p++;
                Value val=parse(s,p);
                m.push_back({key,val});
                skipWS(s,p);
                if(p<s.size()&&s[p]==','){p++;continue;}
                break;
            }
            skipWS(s,p);
            if(p>=s.size()||s[p]!='}')die("Expected '}' in JSON");
            p++;
            return Value::Map(std::move(m));
        }
        if(c=='['){ // array
            p++;skipWS(s,p);
            VVec l;
            if(p<s.size()&&s[p]==']'){p++;return Value::List(std::move(l));}
            while(true){
                l.push_back(parse(s,p));
                skipWS(s,p);
                if(p<s.size()&&s[p]==','){p++;continue;}
                break;
            }
            skipWS(s,p);
            if(p>=s.size()||s[p]!=']')die("Expected ']' in JSON");
            p++;
            return Value::List(std::move(l));
        }
        if(c=='t'){if(s.compare(p,4,"true")==0){p+=4;return Value::Bool(true);}die("Bad JSON token");}
        if(c=='f'){if(s.compare(p,5,"false")==0){p+=5;return Value::Bool(false);}die("Bad JSON token");}
        if(c=='n'){if(s.compare(p,4,"null")==0){p+=4;return Value::Null();}die("Bad JSON token");}
        // number
        if(c=='-'||c=='.'||(c>='0'&&c<='9')){
            size_t start=p;
            if(s[p]=='-')p++;
            while(p<s.size()&&s[p]>='0'&&s[p]<='9')p++;
            if(p<s.size()&&s[p]=='.'){p++;while(p<s.size()&&s[p]>='0'&&s[p]<='9')p++;}
            if(p<s.size()&&(s[p]=='e'||s[p]=='E')){p++;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))p++;
                while(p<s.size()&&s[p]>='0'&&s[p]<='9')p++;}
            return Value::Num(std::stod(s.substr(start,p-start)));
        }
        die("Unexpected character in JSON: '"+std::string(1,c)+"'");
        return Value::Null();
    }
}

// ===== Interpreter =====
struct Interp{
    std::shared_ptr<Env> G;
    std::unordered_map<std::string,Value> moduleCache;
    std::unordered_set<std::string> moduleLoading;

    Interp(){
        G=std::make_shared<Env>();
#ifdef _WIN32
        WSADATA wd;WSAStartup(MAKEWORD(2,2),&wd);
#endif
        initBuiltins();
    }
    ~Interp(){
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void run(const std::vector<NP>&prog){run(prog,G);}
    void run(const std::vector<NP>&prog,std::shared_ptr<Env> env){for(auto&s:prog)exec(s,env);}

    std::vector<NP> parseSource(const std::string&src){
        Lexer lex(src);
        Parser par(lex.toks);
        return par.program();
    }

    std::string readFileText(const fs::path&path){
        std::ifstream f(path,std::ios::binary);
        if(!f)die("Cannot open "+path.string());
        return std::string((std::istreambuf_iterator<char>(f)),{});
    }

    std::string normalizePath(fs::path path){
        path=fs::absolute(path).lexically_normal();
        return path.string();
    }

    std::string resolvePath(const std::string&spec,std::shared_ptr<Env> env,int l){
        if(spec.empty())die("import path cannot be empty",l);
        fs::path p(spec);
        if(p.is_relative()){
            fs::path base=(env&&!env->dir.empty())?fs::path(env->dir):fs::current_path();
            p=base/p;
        }
        p=fs::absolute(p).lexically_normal();
        if(!fs::exists(p)&&!p.has_extension()){
            fs::path alt=p;
            alt+=".sl";
            if(fs::exists(alt))p=alt;
        }
        if(!fs::exists(p))die("Cannot import "+spec+" -> "+p.string(),l);
        return normalizePath(p);
    }

    void bindSource(std::shared_ptr<Env> env,const std::string&path){
        env->file=normalizePath(path);
        env->dir=fs::path(env->file).parent_path().string();
        env->set("__file",Value::Str(env->file));
        env->set("__dir",Value::Str(env->dir));
    }

    Value mkNative(std::function<Value(std::vector<Value>&,std::shared_ptr<Env>,int)> fn){
        auto cl=std::make_shared<Closure>();
        cl->native=std::move(fn);
        return Value::Fn(cl);
    }

    Value* mapFind(Value& v,const std::string& key){
        if(v.ty!=Value::MAP)return nullptr;
        for(auto&[k,val]:v.map())if(k==key)return &val;
        return nullptr;
    }

    const Value* mapFind(const Value& v,const std::string& key)const{
        if(v.ty!=Value::MAP)return nullptr;
        for(auto&[k,val]:v.map())if(k==key)return &val;
        return nullptr;
    }

    std::string needStrArg(const Value& v,const char* msg,int l){
        if(v.ty!=Value::STR)die(msg,l);
        return v.s;
    }

    std::vector<std::string> needStrListArg(const Value& v,const char* msg,int l){
        if(v.ty!=Value::LIST)die(msg,l);
        std::vector<std::string> out;
        for(auto&item:v.list()){
            if(item.ty!=Value::STR)die(msg,l);
            out.push_back(item.s);
        }
        if(out.empty())die(msg,l);
        return out;
    }

    Value exportEnv(const std::shared_ptr<Env>&env){
        VMap out;
        for(auto&[k,v]:env->v){
            if(!k.empty()&&k[0]=='_')continue;
            out.push_back({k,v});
        }
        return Value::Map(std::move(out));
    }

    Value importModule(const std::string&spec,std::shared_ptr<Env> env,int l){
        auto path=resolvePath(spec,env,l);
        auto it=moduleCache.find(path);
        if(it!=moduleCache.end())return it->second;
        if(moduleLoading.count(path))die("Circular import: "+path,l);

        moduleLoading.insert(path);
        auto modEnv=std::make_shared<Env>(G);
        bindSource(modEnv,path);
        try{
            auto prog=parseSource(readFileText(path));
            run(prog,modEnv);
        }catch(...){
            moduleLoading.erase(path);
            throw;
        }
        moduleLoading.erase(path);

        auto exports=exportEnv(modEnv);
        moduleCache[path]=exports;
        return exports;
    }

    void runFile(const std::string&path){
        auto full=resolvePath(path,nullptr,0);
        auto env=std::make_shared<Env>(G);
        bindSource(env,full);
        auto prog=parseSource(readFileText(full));
        run(prog,env);
    }

    // ===== Exec =====
    void exec(NP n,std::shared_ptr<Env> env){
        switch(n->k){
        case Node::sBIND:env->set(n->s,eval(n->ch[0],env));break;
        case Node::sASGN:{auto val=eval(n->ch[1],env);assignTo(n->ch[0],val,env);break;}
        case Node::sFN:{auto cl=std::make_shared<Closure>();cl->params=n->params;cl->body=n->ch[0];cl->env=env;
            env->set(n->s,Value::Fn(cl));break;}
        case Node::sIF:{auto c=eval(n->ch[0],env);
            if(c.IsTrue())exec(n->ch[1],env);
            else if(n->ch.size()>2)exec(n->ch[2],env);
            break;}
        case Node::sWHILE:while(eval(n->ch[0],env).IsTrue()){
            try{exec(n->ch[1],env);}catch(BrkSig&){break;}catch(ContSig&){continue;}}break;
        case Node::sFOR:{auto it=eval(n->ch[0],env);
            if(it.ty!=Value::LIST)die("for requires list",n->ln);
            auto&list=it.list();
            for(int i=0;i<(int)list.size();i++){env->set(n->s,list[i]);
                try{exec(n->ch[1],env);}catch(BrkSig&){break;}catch(ContSig&){continue;}}
            break;}
        case Node::sRET:throw RetSig{n->ch.empty()?Value::Null():eval(n->ch[0],env)};
        case Node::sBRK:throw BrkSig{};
        case Node::sCONT:throw ContSig{};
        case Node::sEXPR:eval(n->ch[0],env);break;
        case Node::sBLOCK:for(auto&s:n->ch)exec(s,env);break;
        case Node::sCTX:{
            auto ctx=eval(n->ch[0],env);
            auto ctxEnv=std::make_shared<Env>(env);
            ctxEnv->set("__ctx",ctx);
            exec(n->ch[1],ctxEnv);
            break;
        }
        default:die("Bad stmt",n->ln);
        }
    }

    void assignTo(NP t,Value&val,std::shared_ptr<Env> env){
        if(t->k==Node::eOUTER){
            auto dst=env->findOuterEnv(t->s);
            if(!dst)die("Undefined outer binding: "+t->s,t->ln);
            dst->set(t->s,val);
        }else if(t->k==Node::eMEM){
            auto base=eval(t->ch[0],env);
            if(base.ty!=Value::MAP)die("Cannot set member on "+std::string(base.TyName()),t->ln);
            auto&m=base.map();
            for(auto&[k,v]:m)if(k==t->s){v=val;return;}
            m.push_back({t->s,val});
        }else if(t->k==Node::eIDX){
            auto base=eval(t->ch[0],env);auto idx=eval(t->ch[1],env);
            if(base.ty==Value::LIST){
                int i=intIndex(idx,"list assignment requires integer index",t->ln);
                auto&l=base.list();
                if(i<0||i>=(int)l.size())die("Index out of range",t->ln);
                l[i]=val;
            }else if(base.ty==Value::MAP){
                if(idx.ty!=Value::STR)die("map assignment requires string key",t->ln);
                auto&m=base.map();
                for(auto&[k,v]:m){
                    if(k==idx.s){v=val;return;}
                }
                m.push_back({idx.s,val});
            }
            else die("Cannot index "+std::string(base.TyName()),t->ln);
        }else die("Bad assign target",t->ln);
    }

    // ===== Eval =====
    Value eval(NP n,std::shared_ptr<Env> env){
        switch(n->k){
        case Node::eNUM:return Value::Num(n->num);
        case Node::eSTR:return Value::Str(n->s);
        case Node::eBOOL:return Value::Bool(n->num!=0);
        case Node::eNUL:return Value::Null();
        case Node::eID:{auto*v=env->find(n->s);if(!v)die("Undefined: "+n->s,n->ln);return*v;}
        case Node::eOUTER:{auto*v=env->findOuter(n->s);if(!v)die("Undefined outer binding: "+n->s,n->ln);return*v;}
        case Node::eCTX:{auto*v=env->findCtx();if(!v)die("No current context",n->ln);return*v;}
        case Node::ePH:die("Placeholder escaped lowering",n->ln);
        case Node::eLIST:{VVec items;for(auto&c:n->ch)items.push_back(eval(c,env));return Value::List(std::move(items));}
        case Node::eMAP:{VMap ents;for(auto&[k,v]:n->entries)ents.push_back({k,eval(v,env)});return Value::Map(std::move(ents));}
        case Node::eUN:return evalUn(n,env);
        case Node::eBIN:return evalBin(n,env);
        case Node::eMEM:{auto b=eval(n->ch[0],env);return memAcc(b,n->s,n->ln);}
        case Node::eSMEM:{auto b=eval(n->ch[0],env);if(b.ty==Value::NUL)return Value::Null();return memAcc(b,n->s,n->ln);}
        case Node::eIDX:{auto b=eval(n->ch[0],env);auto i=eval(n->ch[1],env);return idxAcc(b,i,n->ln);}
        case Node::eSIDX:{auto b=eval(n->ch[0],env);if(b.ty==Value::NUL)return Value::Null();auto i=eval(n->ch[1],env);return safeIdxAcc(b,i,n->ln);}
        case Node::eCALL:return evalCall(n,env);
        case Node::eSPLIT:{auto c=eval(n->ch[0],env);return c.IsTrue()?eval(n->ch[1],env):eval(n->ch[2],env);}
        case Node::eLAM:{auto cl=std::make_shared<Closure>();cl->params=n->params;cl->body=n->ch[0];cl->env=env;return Value::Fn(cl);}
        case Node::eCTXEXPR:{
            auto ctx=eval(n->ch[0],env);
            auto ctxEnv=std::make_shared<Env>(env);
            ctxEnv->set("__ctx",ctx);
            return eval(n->ch[1],ctxEnv);
        }
        case Node::eDERIV:return evalDeriv(n,env);
        case Node::eLINK:return evalLink(n,env);
        default:die("Bad expr",n->ln);
        }
        return Value::Null();
    }

    Value evalUn(NP n,std::shared_ptr<Env> env){
        auto v=eval(n->ch[0],env);
        switch(n->op){
        case tMINUS:if(v.ty!=Value::NUM)die("- requires number",n->ln);return Value::Num(-v.n);
        case tNOT:return Value::Bool(!v.IsTrue());
        default:die("Bad unary",n->ln);
        }
        return Value::Null();
    }

    Value evalBin(NP n,std::shared_ptr<Env> env){
        if(n->op==tAND){auto l=eval(n->ch[0],env);return l.IsTrue()?eval(n->ch[1],env):l;}
        if(n->op==tOR){auto l=eval(n->ch[0],env);return l.IsTrue()?l:eval(n->ch[1],env);}
        auto l=eval(n->ch[0],env),r=eval(n->ch[1],env);
        switch(n->op){
        case tPLUS:
            if(l.ty==Value::NUM&&r.ty==Value::NUM)return Value::Num(l.n+r.n);
            if(l.ty==Value::STR||r.ty==Value::STR)return Value::Str(l.ToStr()+r.ToStr());
            die("Cannot + "+std::string(l.TyName())+" and "+r.TyName(),n->ln);
        case tMINUS:return Value::Num(numV(l,n->ln)-numV(r,n->ln));
        case tSTAR:return Value::Num(numV(l,n->ln)*numV(r,n->ln));
        case tSLASH:{double d=numV(r,n->ln);if(d==0)die("Division by zero",n->ln);return Value::Num(numV(l,n->ln)/d);}
        case tPCT:{double d=numV(r,n->ln);if(d==0)die("Modulo by zero",n->ln);return Value::Num(std::fmod(numV(l,n->ln),d));}
        case tEQEQ:return Value::Bool(l==r);
        case tNEQ:return Value::Bool(l!=r);
        case tLT:return Value::Bool(cmpLt(l,r,n->ln));
        case tLE:return Value::Bool(!cmpLt(r,l,n->ln));
        case tGT:return Value::Bool(cmpLt(r,l,n->ln));
        case tGE:return Value::Bool(!cmpLt(l,r,n->ln));
        default:die("Bad binop",n->ln);
        }
        return Value::Null();
    }

    double numV(const Value&v,int l){if(v.ty!=Value::NUM)die("Expected number, got "+std::string(v.TyName()),l);return v.n;}
    int intIndex(const Value&v,const char*msg,int l){
        if(v.ty!=Value::NUM)die(msg,l);
        double n=v.n;
        if(std::floor(n)!=n)die(msg,l);
        if(n<(double)std::numeric_limits<int>::min()||n>(double)std::numeric_limits<int>::max())die(msg,l);
        return (int)n;
    }
    bool cmpLt(const Value&a,const Value&b,int l){
        if(a.ty==Value::NUM&&b.ty==Value::NUM)return a.n<b.n;
        if(a.ty==Value::STR&&b.ty==Value::STR)return a.s<b.s;
        die("Cannot compare "+std::string(a.TyName())+" and "+b.TyName(),l);return false;
    }

    Value memAcc(Value&b,const std::string&m,int l){
        if(b.ty==Value::MAP){
            auto&mp=b.map();
            for(auto&[k,v]:mp){
                if(k==m)return v;
            }
            return Value::Null();
        }
        die("No property '"+m+"' on "+std::string(b.TyName()),l);return Value::Null();
    }
    Value idxAcc(Value&b,Value&i,int l){
        if(b.ty==Value::LIST){
            int idx=intIndex(i,"list index requires integer index",l);
            auto&list=b.list();
            if(idx<0||idx>=(int)list.size())die("Index out of range",l);
            return list[idx];
        }
        if(b.ty==Value::MAP){
            if(i.ty!=Value::STR)die("map index requires string key",l);
            auto&mp=b.map();
            for(auto&[k,v]:mp){
                if(k==i.s)return v;
            }
            return Value::Null();
        }
        if(b.ty==Value::STR){
            int idx=intIndex(i,"string index requires integer index",l);
            if(idx<0||idx>=(int)b.s.size())die("Index out of range",l);
            return Value::Str(std::string(1,b.s[idx]));}
        die("Cannot index "+std::string(b.TyName()),l);return Value::Null();
    }
    Value safeIdxAcc(Value&b,Value&i,int l){
        if(b.ty==Value::LIST){
            int idx=intIndex(i,"list index requires integer index",l);
            auto&list=b.list();
            if(idx<0||idx>=(int)list.size())return Value::Null();
            return list[idx];
        }
        if(b.ty==Value::MAP){
            if(i.ty!=Value::STR)die("map index requires string key",l);
            auto&mp=b.map();
            for(auto&[k,v]:mp){
                if(k==i.s)return v;
            }
            return Value::Null();
        }
        if(b.ty==Value::STR){
            int idx=intIndex(i,"string index requires integer index",l);
            if(idx<0||idx>=(int)b.s.size())return Value::Null();
            return Value::Str(std::string(1,b.s[idx]));}
        die("Cannot index "+std::string(b.TyName()),l);return Value::Null();
    }

    // ===== Call =====
    Value evalCall(NP n,std::shared_ptr<Env> env){
        auto&cn=n->ch[0];
        std::vector<Value> args;
        for(int i=1;i<(int)n->ch.size();i++)args.push_back(eval(n->ch[i],env));
        // method call
        if(cn->k==Node::eMEM||cn->k==Node::eSMEM){
            auto base=eval(cn->ch[0],env);
            if(cn->k==Node::eSMEM&&base.ty==Value::NUL)return Value::Null();
            return callMethod(base,cn->s,args,n->ln,env);
        }
        auto callee=eval(cn,env);
        return callFn(callee,args,n->ln,env);
    }

    Value callFn(Value&f,std::vector<Value>&args,int l,std::shared_ptr<Env> callerEnv=nullptr){
        if(f.ty!=Value::FN)die("Not callable: "+std::string(f.TyName()),l);
        auto&cl=f.fn();
        if(cl.isNat())return cl.native(args,callerEnv,l);
        auto fe=std::make_shared<Env>(cl.env);
        for(int i=0;i<(int)cl.params.size();i++)
            fe->set(cl.params[i],i<(int)args.size()?args[i]:Value::Null());
        try{exec(cl.body,fe);}
        catch(RetSig&rs){return rs.val;}
        catch(BrkSig&){die("break outside loop",l);}
        catch(ContSig&){die("continue outside loop",l);}
        return Value::Null();
    }

    // ===== Method dispatch =====
    Value callMethod(Value&base,const std::string&m,std::vector<Value>&a,int l,std::shared_ptr<Env> env){
        switch(base.ty){
        case Value::LIST:return listM(base,m,a,l,env);
        case Value::STR:return strM(base,m,a,l);
        case Value::MAP:return mapM(base,m,a,l,env);
        case Value::REF:return refM(base,m,a,l);
        case Value::TCP:return tcpM(base,m,a,l);
        case Value::TCPS:return tcpSM(base,m,a,l);
        case Value::PIPE:return pipeM(base,m,a,l);
        case Value::SHM:return shmM(base,m,a,l);
        case Value::PROC:return procM(base,m,a,l);
        default:break;
        }
        // fallback: map member that is a function
        if(base.ty==Value::MAP){auto&mp=base.map();
            for(auto&[k,v]:mp)if(k==m&&v.ty==Value::FN){return callFn(v,a,l,env);}
        }
        die("No method '"+m+"' on "+std::string(base.TyName()),l);return Value::Null();
    }

    // ===== List methods =====
    Value listM(Value&b,const std::string&m,std::vector<Value>&a,int l,std::shared_ptr<Env> env){
        auto&ls=b.list();
        if(m=="push"){if(a.size()<1)die("list.push requires 1 arg",l);ls.push_back(a[0]);return Value::Null();}
        if(m=="pop"){if(ls.empty())die("pop on empty list",l);auto v=ls.back();ls.pop_back();return v;}
        if(m=="slice"){if(a.size()<2)die("list.slice requires 2 args",l);int s=(int)a[0].n,e=(int)a[1].n;
            s=std::max(0,s);e=std::min(e,(int)ls.size());
            return Value::List(VVec(ls.begin()+s,ls.begin()+e));}
        if(m=="map"){if(a.size()<1)die("list.map requires 1 arg",l);VVec r;for(auto&el:ls){std::vector<Value>ar{el};r.push_back(callFn(a[0],ar,l,env));}
            return Value::List(std::move(r));}
        if(m=="filter"){if(a.size()<1)die("list.filter requires 1 arg",l);VVec r;for(auto&el:ls){std::vector<Value>ar{el};if(callFn(a[0],ar,l,env).IsTrue())r.push_back(el);}
            return Value::List(std::move(r));}
        if(m=="reduce"){if(a.size()<2)die("list.reduce requires 2 args",l);Value acc=a[1];for(auto&el:ls){std::vector<Value>ar{acc,el};acc=callFn(a[0],ar,l,env);}return acc;}
        if(m=="find"){if(a.size()<1)die("list.find requires 1 arg",l);for(auto&el:ls){std::vector<Value>ar{el};if(callFn(a[0],ar,l,env).IsTrue())return el;}return Value::Null();}
        if(m=="contains"){if(a.size()<1)die("list.contains requires 1 arg",l);for(auto&el:ls)if(el==a[0])return Value::Bool(true);return Value::Bool(false);}
        if(m=="join"){if(a.size()<1||a[0].ty!=Value::STR)die("list.join requires string separator",l);std::string r;for(size_t i=0;i<ls.size();i++){if(i)r+=a[0].s;r+=ls[i].ToStr();}return Value::Str(r);}
        if(m=="reverse"){return Value::List(VVec(ls.rbegin(),ls.rend()));}
        die("Unknown list method: "+m,l);return Value::Null();
    }

    // ===== String methods =====
    Value strM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&s=b.s;
        if(m=="split"){if(a.size()<1||a[0].ty!=Value::STR)die("string.split requires string separator",l);std::vector<Value>r;auto&sep=a[0].s;if(sep.empty())die("string.split: separator cannot be empty",l);size_t pos=0;
            while(true){auto f=s.find(sep,pos);if(f==std::string::npos){r.push_back(Value::Str(s.substr(pos)));break;}
                r.push_back(Value::Str(s.substr(pos,f-pos)));pos=f+sep.size();}
            return Value::List(VVec(r.begin(),r.end()));}
        if(m=="trim"){size_t a0=s.find_first_not_of(" \t\n\r"),b0=s.find_last_not_of(" \t\n\r");
            return a0==std::string::npos?Value::Str(""):Value::Str(s.substr(a0,b0-a0+1));}
        if(m=="chars"){VVec r;for(char c:s)r.push_back(Value::Str(std::string(1,c)));return Value::List(std::move(r));}
        if(m=="starts_with"){if(a.size()<1||a[0].ty!=Value::STR)die("string.starts_with requires string prefix",l);return Value::Bool(s.size()>=a[0].s.size()&&s.compare(0,a[0].s.size(),a[0].s)==0);}
        if(m=="ends_with"){if(a.size()<1||a[0].ty!=Value::STR)die("string.ends_with requires string suffix",l);return Value::Bool(s.size()>=a[0].s.size()&&s.compare(s.size()-a[0].s.size(),a[0].s.size(),a[0].s)==0);}
        if(m=="contains"){if(a.size()<1||a[0].ty!=Value::STR)die("string.contains requires string part",l);return Value::Bool(s.find(a[0].s)!=std::string::npos);}
        if(m=="replace"){if(a.size()<2||a[0].ty!=Value::STR||a[1].ty!=Value::STR)die("string.replace requires 2 string args",l);std::string r=s;auto&old_=a[0].s;auto&new_=a[1].s;if(old_.empty())die("string.replace: old value cannot be empty",l);size_t p=0;
            while((p=r.find(old_,p))!=std::string::npos){r.replace(p,old_.size(),new_);p+=new_.size();}
            return Value::Str(r);}
        die("Unknown string method: "+m,l);return Value::Null();
    }

    // ===== Map methods =====
    Value mapM(Value&b,const std::string&m,std::vector<Value>&a,int l,std::shared_ptr<Env> env){
        auto&mp=b.map();
        if(m=="keys"){VVec r;for(auto&[k,v]:mp)r.push_back(Value::Str(k));return Value::List(std::move(r));}
        if(m=="values"){VVec r;for(auto&[k,v]:mp)r.push_back(v);return Value::List(std::move(r));}
        if(m=="has"){if(a.size()<1||a[0].ty!=Value::STR)die("map.has requires string key",l);for(auto&[k,v]:mp)if(k==a[0].s)return Value::Bool(true);return Value::Bool(false);}
        if(m=="remove"){
            if(a.size()<1||a[0].ty!=Value::STR)die("map.remove requires string key",l);
            for(auto it=mp.begin();it!=mp.end();++it){
                if(it->first==a[0].s){
                    auto v=it->second;
                    mp.erase(it);
                    return v;
                }
            }
            return Value::Null();
        }
        // fallback: call function stored in map
        for(auto&[k,v]:mp)if(k==m&&v.ty==Value::FN)return callFn(v,a,l,env);
        die("Unknown map method: "+m,l);return Value::Null();
    }

    // ===== Ref methods =====
    Value refM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&h=b.ref();
        if(m=="deref"){
            switch(h.kind){
            case RefHandle::VAR:{auto*v=h.env->find(h.name);if(!v)die("Ref dangling",l);return*v;}
            case RefHandle::LREF:{if(h.idx<0||h.idx>=(int)h.list->size())die("Ref OOB",l);return(*h.list)[h.idx];}
            case RefHandle::MREF:{for(auto&[k,v]:*h.map)if(k==h.key)return v;return Value::Null();}
            }
        }
        if(m=="set"){
            if(a.size()<1)die("ref.set requires 1 arg",l);
            switch(h.kind){
            case RefHandle::VAR:h.env->set(h.name,a[0]);break;
            case RefHandle::LREF:{if(h.idx<0||h.idx>=(int)h.list->size())die("Ref OOB",l);(*h.list)[h.idx]=a[0];break;}
            case RefHandle::MREF:{for(auto&[k,v]:*h.map)if(k==h.key){v=a[0];return Value::Null();}
                h.map->push_back({h.key,a[0]});break;}
            }
            return Value::Null();
        }
        die("Unknown ref method: "+m,l);return Value::Null();
    }

    // ===== TCP methods =====
    Value tcpM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&h=b.tcp();
        if(h.closed){
            if(m=="close")return Value::Null();
            die("Operation on closed tcp",l);
        }
        if(m=="send"){if(a.empty()||a[0].ty!=Value::NET)die("send requires net-form",l);
            pnet::sendMsg(h.fd,a[0].net().data);return Value::Null();}
        if(m=="recv"){auto data=pnet::recvMsg(h.fd);size_t pos=0;return deser(data,pos);}
        if(m=="send_raw"){if(a.empty()||a[0].ty!=Value::STR)die("tcp.send_raw requires string",l);
            pnet::sendAll(h.fd,a[0].s.c_str(),(int)a[0].s.size());return Value::Null();}
        if(m=="recv_raw"){if(a.empty()||a[0].ty!=Value::NUM)die("tcp.recv_raw requires number",l);
            int n=(int)a[0].n;if(n<0)die("tcp.recv_raw requires non-negative size",l);std::string buf(n,0);pnet::recvAll(h.fd,&buf[0],n);return Value::Str(buf);}
        if(m=="peer_addr"){
            struct sockaddr_storage sa{};socklen_t sl=sizeof(sa);
            if(getpeername((int)h.fd,(struct sockaddr*)&sa,&sl)!=0)return Value::Null();
            char host[NI_MAXHOST],port[NI_MAXSERV];
            getnameinfo((struct sockaddr*)&sa,sl,host,sizeof(host),port,sizeof(port),NI_NUMERICHOST|NI_NUMERICSERV);
            return Value::Str(std::string(host)+":"+port);
        }
        if(m=="set_timeout"){if(a.empty()||a[0].ty!=Value::NUM)die("set_timeout requires number",l);
            int secs=(int)a[0].n;if(secs<0)die("tcp.set_timeout requires non-negative seconds",l);
#ifdef _WIN32
            DWORD ms=secs*1000;
            setsockopt((SOCKET)h.fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&ms,sizeof(ms));
            setsockopt((SOCKET)h.fd,SOL_SOCKET,SO_SNDTIMEO,(char*)&ms,sizeof(ms));
#else
            struct timeval tv;tv.tv_sec=secs;tv.tv_usec=0;
            setsockopt((int)h.fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
            setsockopt((int)h.fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
#endif
            return Value::Null();}
        if(m=="close"){
#ifdef _WIN32
            closesocket((SOCKET)h.fd);
#else
            ::close((int)h.fd);
#endif
            h.closed=true;return Value::Null();}
        die("Unknown tcp method: "+m,l);return Value::Null();
    }

    // ===== TCP Server methods =====
    Value tcpSM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&h=b.tcps();
        if(h.closed){
            if(m=="close")return Value::Null();
            die("Operation on closed tcp-server",l);
        }
        if(m=="accept"){
            struct sockaddr_storage sa{};socklen_t sl=sizeof(sa);
#ifdef _WIN32
            SOCKET client=::accept((SOCKET)h.fd,(struct sockaddr*)&sa,&sl);
            if(client==INVALID_SOCKET)die("accept failed"+pnet::sysErr(),l);
            auto ch=std::shared_ptr<TcpHandle>(new TcpHandle{(uintptr_t)client,false},[](TcpHandle*p){
                if(!p->closed){closesocket((SOCKET)p->fd);}delete p;});
#else
            int client=::accept((int)h.fd,(struct sockaddr*)&sa,&sl);
            if(client<0)die("accept failed"+pnet::sysErr(),l);
            auto ch=std::shared_ptr<TcpHandle>(new TcpHandle{(uintptr_t)client,false},[](TcpHandle*p){
                if(!p->closed){::close((int)p->fd);}delete p;});
#endif
            return Value::MkTcp(ch);
        }
        if(m=="addr"){
            struct sockaddr_storage sa{};socklen_t sl=sizeof(sa);
            if(getsockname((int)h.fd,(struct sockaddr*)&sa,&sl)!=0)return Value::Null();
            char host[NI_MAXHOST],port[NI_MAXSERV];
            getnameinfo((struct sockaddr*)&sa,sl,host,sizeof(host),port,sizeof(port),NI_NUMERICHOST|NI_NUMERICSERV);
            return Value::Str(std::string(host)+":"+port);
        }
        if(m=="close"){
#ifdef _WIN32
            closesocket((SOCKET)h.fd);
#else
            ::close((int)h.fd);
#endif
            h.closed=true;return Value::Null();}
        die("Unknown tcp-server method: "+m,l);return Value::Null();
    }

    // ===== Pipe methods =====
    Value pipeM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&h=b.pipe();
        if(h.closed){
            if(m=="close")return Value::Null(); // idempotent close
            die("Operation on closed pipe",l);
        }
        if(m=="send"){if(a.empty()||a[0].ty!=Value::NET)die("send requires net-form",l);
            pnet::pipeSendMsg(h.wh,a[0].net().data);return Value::Null();}
        if(m=="recv"){auto data=pnet::pipeRecvMsg(h.rh);size_t pos=0;return deser(data,pos);}
        if(m=="is_named")return Value::Bool(h.named);
        if(m=="name")return h.named?Value::Str(h.name):Value::Null();
        if(m=="flush"){
#ifdef _WIN32
            FlushFileBuffers((HANDLE)h.wh);
#else
            fsync((int)h.wh);
#endif
            return Value::Null();}
        if(m=="close"){
#ifdef _WIN32
            if(h.rh)CloseHandle((HANDLE)h.rh);
            if(h.wh&&h.wh!=h.rh)CloseHandle((HANDLE)h.wh);
#else
            if(h.rh)::close((int)h.rh);
            if(h.wh&&h.wh!=h.rh)::close((int)h.wh);
            if(h.named)::unlink(("/tmp/sl_pipe_"+h.name).c_str());
#endif
            h.closed=true;return Value::Null();}
        die("Unknown pipe method: "+m,l);return Value::Null();
    }

    // ===== SHM methods =====
    Value shmM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&h=b.shm();
        if(h.closed){
            if(m=="close")return Value::Null();
            die("Operation on closed shm",l);
        }
        if(m=="store"){if(a.empty()||a[0].ty!=Value::NET)die("store requires net-form",l);
            auto&data=a[0].net().data;
            uint32_t len=(uint32_t)data.size();
            if(len+8>(uint32_t)h.sz)die("shm data too large",l);
            pnet::shmLock(h.ptr);
            pnet::writeU32LE((char*)h.ptr+4,len);
            std::memcpy((char*)h.ptr+8,data.c_str(),len);
            pnet::shmUnlock(h.ptr);
            return Value::Null();}
        if(m=="load"){
            pnet::shmLock(h.ptr);
            uint32_t len=pnet::readU32LE((char*)h.ptr+4);
            if(len==0){pnet::shmUnlock(h.ptr);return Value::Null();}
            std::string data((char*)h.ptr+8,len);
            pnet::shmUnlock(h.ptr);
            size_t pos=0;return deser(data,pos);}
        if(m=="size")return Value::Num((double)h.sz);
        if(m=="name")return Value::Str(h.name);
        if(m=="clear"){
            pnet::shmLock(h.ptr);
            pnet::writeU32LE((char*)h.ptr+4,0);
            pnet::shmUnlock(h.ptr);
            return Value::Null();}
        if(m=="close"){
#ifdef _WIN32
            UnmapViewOfFile(h.ptr);CloseHandle((HANDLE)h.hm);
#else
            munmap(h.ptr,h.sz);::close((int)h.hm);
            if(h.owner)shm_unlink(("/sl_shm_"+h.name).c_str());
#endif
            h.closed=true;return Value::Null();}
        die("Unknown shm method: "+m,l);return Value::Null();
    }

    Value procM(Value&b,const std::string&m,std::vector<Value>&a,int l){
        auto&h=b.proc();
        auto finishProc=[&](int code)->Value{
            h.waited=true;
            h.exit_code=code;
#ifdef _WIN32
            if(!h.closed&&h.h){CloseHandle((HANDLE)h.h);}
#else
            (void)h;
#endif
            h.closed=true;
            h.h=0;
            return Value::Num((double)code);
        };
        if(m=="pid")return Value::Num((double)h.pid);
        if(m=="is_alive"){
            if(h.waited)return Value::Bool(false);
#ifdef _WIN32
            if(h.closed||!h.h)return Value::Bool(false);
            DWORD rc=WaitForSingleObject((HANDLE)h.h,0);
            if(rc==WAIT_TIMEOUT)return Value::Bool(true);
            if(rc==WAIT_OBJECT_0){
                DWORD code=0;
                GetExitCodeProcess((HANDLE)h.h,&code);
                h.waited=true;h.exit_code=(int)code;CloseHandle((HANDLE)h.h);h.h=0;h.closed=true;
                return Value::Bool(false);
            }
            die("process.is_alive failed"+pnet::sysErr(),l);
#else
            if(h.closed)return Value::Bool(false);
            int status=0;
            pid_t rc=waitpid((pid_t)h.pid,&status,WNOHANG);
            if(rc==0)return Value::Bool(true);
            if(rc<0){
                if(errno==ECHILD){h.waited=true;h.closed=true;return Value::Bool(false);}
                die("process.is_alive failed"+pnet::sysErr(),l);
            }
            int code=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
            h.waited=true;h.exit_code=code;h.closed=true;
            return Value::Bool(false);
#endif
        }
        if(m=="wait"){
            if(h.waited)return Value::Num((double)h.exit_code);
            int timeout_ms=-1;
            if(!a.empty()){
                if(a[0].ty!=Value::NUM)die("process.wait requires number timeout seconds",l);
                if(a[0].n<0)die("process.wait requires non-negative timeout seconds",l);
                timeout_ms=(int)(a[0].n*1000.0);
            }
#ifdef _WIN32
            if(h.closed||!h.h)return Value::Num((double)h.exit_code);
            DWORD rc=WaitForSingleObject((HANDLE)h.h,timeout_ms<0?INFINITE:(DWORD)timeout_ms);
            if(rc==WAIT_TIMEOUT)return Value::Null();
            if(rc!=WAIT_OBJECT_0)die("process.wait failed"+pnet::sysErr(),l);
            DWORD code=0;
            if(!GetExitCodeProcess((HANDLE)h.h,&code))die("process.wait failed"+pnet::sysErr(),l);
            return finishProc((int)code);
#else
            if(h.closed)return Value::Num((double)h.exit_code);
            if(timeout_ms<0){
                int status=0;
                pid_t rc=waitpid((pid_t)h.pid,&status,0);
                if(rc<0)die("process.wait failed"+pnet::sysErr(),l);
                int code=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
                return finishProc(code);
            }
            auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
            for(;;){
                int status=0;
                pid_t rc=waitpid((pid_t)h.pid,&status,WNOHANG);
                if(rc<0)die("process.wait failed"+pnet::sysErr(),l);
                if(rc>0){
                    int code=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
                    return finishProc(code);
                }
                if(std::chrono::steady_clock::now()>=deadline)return Value::Null();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
#endif
        }
        if(m=="terminate"||m=="kill"){
            if(h.waited)return Value::Null();
#ifdef _WIN32
            if(h.closed||!h.h)return Value::Null();
            if(!TerminateProcess((HANDLE)h.h,1))die("process.kill failed"+pnet::sysErr(),l);
            DWORD code=0;
            WaitForSingleObject((HANDLE)h.h,INFINITE);
            GetExitCodeProcess((HANDLE)h.h,&code);
            return finishProc((int)code);
#else
            if(h.closed)return Value::Null();
            if(::kill((pid_t)h.pid,SIGTERM)!=0)die("process.kill failed"+pnet::sysErr(),l);
            int status=0;
            if(waitpid((pid_t)h.pid,&status,0)<0)die("process.kill failed"+pnet::sysErr(),l);
            int code=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):-1);
            return finishProc(code);
#endif
        }
        die("Unknown process method: "+m,l);return Value::Null();
    }

    // ===== Derive =====
    Value deepCopy(const Value&v){
        switch(v.ty){
        case Value::LIST:{VVec r;for(auto&e:v.list())r.push_back(deepCopy(e));return Value::List(std::move(r));}
        case Value::MAP:{VMap r;for(auto&[k,val]:v.map())r.push_back({k,deepCopy(val)});return Value::Map(std::move(r));}
        default:return v;
        }
    }

    Value evalDeriv(NP n,std::shared_ptr<Env> env){
        auto v=eval(n->ch[0],env);
        auto&m=n->s;
        if(m=="copy"){
            if(v.ty==Value::LIST)return Value::List(VVec(v.list()));
            if(v.ty==Value::MAP)return Value::Map(VMap(v.map()));
            return v;
        }
        if(m=="deep")return deepCopy(v);
        if(m=="snap")return deepCopy(v); // COW fallback to eager deep
        if(m=="net"){auto nf=std::make_shared<NetForm>();nf->data=ser(v);return Value::MkNet(nf);}
        die("Unknown derive: "+m,n->ln);return Value::Null();
    }

    // ===== Link =====
    Value evalLink(NP n,std::shared_ptr<Env> env){
        if(n->s=="ref")return evalRefLink(n,env);
        if(n->s=="tcp")return evalTcpLink(n,env);
        if(n->s=="pipe")return evalPipeLink(n,env);
        if(n->s=="shm")return evalShmLink(n,env);
        die("Unknown link: "+n->s,n->ln);return Value::Null();
    }

    Value evalRefLink(NP n,std::shared_ptr<Env> env){
        auto&arg=n->ch[0];
        auto h=std::make_shared<RefHandle>();
        if(arg->k==Node::eID){
            h->kind=RefHandle::VAR;
            h->env=env->findEnv(arg->s);
            if(!h->env)die("Undefined: "+arg->s,n->ln);
            h->name=arg->s;
        }else if(arg->k==Node::eOUTER){
            h->kind=RefHandle::VAR;
            h->env=env->findOuterEnv(arg->s);
            if(!h->env)die("Undefined outer binding: "+arg->s,n->ln);
            h->name=arg->s;
        }else if(arg->k==Node::eIDX){
            auto base=eval(arg->ch[0],env);auto idx=eval(arg->ch[1],env);
            if(base.ty==Value::LIST){
                h->kind=RefHandle::LREF;
                h->list=std::static_pointer_cast<VVec>(base.o);
                h->idx=intIndex(idx,"list ref requires integer index",n->ln);
            }
            else if(base.ty==Value::MAP){
                if(idx.ty!=Value::STR)die("map ref requires string key",n->ln);
                h->kind=RefHandle::MREF;h->map=std::static_pointer_cast<VMap>(base.o);h->key=idx.s;
            }
            else die("Cannot ref into "+std::string(base.TyName()),n->ln);
        }else if(arg->k==Node::eMEM){
            auto base=eval(arg->ch[0],env);
            if(base.ty!=Value::MAP)die("Cannot ref member of "+std::string(base.TyName()),n->ln);
            h->kind=RefHandle::MREF;h->map=std::static_pointer_cast<VMap>(base.o);h->key=arg->s;
        }else die("Invalid ref target",n->ln);
        return Value::MkRef(h);
    }

    Value evalTcpLink(NP n,std::shared_ptr<Env> env){
        auto addr=eval(n->ch[0],env);
        if(addr.ty!=Value::STR)die("tcp endpoint must be string",n->ln);
        auto&s=addr.s;auto colon=s.rfind(':');
        if(colon==std::string::npos)die("Invalid endpoint: "+s,n->ln);
        std::string host=s.substr(0,colon);int port=std::stoi(s.substr(colon+1));
        if(port<1||port>65535)die("Port out of range: "+std::to_string(port),n->ln);
        int timeout_s=3;
        if((int)n->ch.size()>1){auto tv=eval(n->ch[1],env);if(tv.ty==Value::NUM)timeout_s=(int)tv.n;}

#ifdef _WIN32
        SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(sock==INVALID_SOCKET)die("socket() failed"+pnet::sysErr(),n->ln);
        // Non-blocking connect with timeout
        u_long nb=1;ioctlsocket(sock,FIONBIO,&nb);
        struct sockaddr_in sa{};sa.sin_family=AF_INET;sa.sin_port=htons((u_short)port);
        inet_pton(AF_INET,host.c_str(),&sa.sin_addr);
        int cr=connect(sock,(struct sockaddr*)&sa,sizeof(sa));
        if(cr!=0&&WSAGetLastError()==WSAEWOULDBLOCK){
            fd_set wset;FD_ZERO(&wset);FD_SET(sock,&wset);
            struct timeval tv{timeout_s,0};
            int sr=select(0,NULL,&wset,NULL,&tv);
            if(sr<=0){closesocket(sock);die("connect timeout: "+s+pnet::sysErr(),n->ln);}
        }else if(cr!=0){closesocket(sock);die("connect failed: "+s+pnet::sysErr(),n->ln);}
        nb=0;ioctlsocket(sock,FIONBIO,&nb); // back to blocking
        auto h=std::shared_ptr<TcpHandle>(new TcpHandle{(uintptr_t)sock,false},[](TcpHandle*p){
            if(!p->closed){closesocket((SOCKET)p->fd);}delete p;});
        return Value::MkTcp(h);
#else
        int sock=socket(AF_INET,SOCK_STREAM,0);
        if(sock<0)die("socket() failed"+pnet::sysErr(),n->ln);
        // Non-blocking connect with timeout
        int flags=fcntl(sock,F_GETFL,0);fcntl(sock,F_SETFL,flags|O_NONBLOCK);
        struct sockaddr_in sa{};sa.sin_family=AF_INET;sa.sin_port=htons(port);
        inet_pton(AF_INET,host.c_str(),&sa.sin_addr);
        int cr=connect(sock,(struct sockaddr*)&sa,sizeof(sa));
        if(cr<0&&errno==EINPROGRESS){
            fd_set wset;FD_ZERO(&wset);FD_SET(sock,&wset);
            struct timeval tv{timeout_s,0};
            int sr=select(sock+1,NULL,&wset,NULL,&tv);
            if(sr<=0){::close(sock);die("connect timeout: "+s+pnet::sysErr(),n->ln);}
            int err=0;socklen_t el=sizeof(err);getsockopt(sock,SOL_SOCKET,SO_ERROR,&err,&el);
            if(err!=0){::close(sock);die("connect failed: "+s+pnet::sysErr(),n->ln);}
        }else if(cr<0){::close(sock);die("connect failed: "+s+pnet::sysErr(),n->ln);}
        fcntl(sock,F_SETFL,flags); // back to blocking
        auto h=std::shared_ptr<TcpHandle>(new TcpHandle{(uintptr_t)sock,false},[](TcpHandle*p){
            if(!p->closed){::close((int)p->fd);}delete p;});
        return Value::MkTcp(h);
#endif
    }

    Value evalPipeLink(NP n,std::shared_ptr<Env> env){
        if(n->ch.empty()){
            // anonymous pipe
#ifdef _WIN32
            HANDLE r,w;
            SECURITY_ATTRIBUTES sa{sizeof(sa),NULL,TRUE};
            if(!CreatePipe(&r,&w,&sa,0))die("CreatePipe failed"+pnet::sysErr(),n->ln);
            auto h=std::shared_ptr<PipeHandle>(new PipeHandle{(uintptr_t)r,(uintptr_t)w,"",false,false},[](PipeHandle*p){
                if(!p->closed){CloseHandle((HANDLE)p->rh);if(p->wh!=p->rh)CloseHandle((HANDLE)p->wh);}delete p;});
#else
            int fd[2];if(::pipe(fd)!=0)die("pipe() failed"+pnet::sysErr(),n->ln);
            auto h=std::shared_ptr<PipeHandle>(new PipeHandle{(uintptr_t)fd[0],(uintptr_t)fd[1],"",false,false},[](PipeHandle*p){
                if(!p->closed){::close((int)p->rh);if(p->wh!=p->rh)::close((int)p->wh);}delete p;});
#endif
            return Value::MkPipe(h);
        }else{
            auto name=eval(n->ch[0],env);
            if(name.ty!=Value::STR)die("pipe name must be string",n->ln);
#ifdef _WIN32
            std::string pn="\\\\.\\pipe\\"+name.s;
            HANDLE ph=CreateNamedPipeA(pn.c_str(),PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,1,4096,4096,0,NULL);
            if(ph==INVALID_HANDLE_VALUE){
                ph=CreateFileA(pn.c_str(),GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
                if(ph==INVALID_HANDLE_VALUE)die("Cannot open pipe: "+name.s+pnet::sysErr(),n->ln);
            }else{ConnectNamedPipe(ph,NULL);}
            auto h=std::shared_ptr<PipeHandle>(new PipeHandle{(uintptr_t)ph,(uintptr_t)ph,name.s,false,true},[](PipeHandle*p){
                if(!p->closed){CloseHandle((HANDLE)p->rh);}delete p;});
#else
            std::string pn="/tmp/sl_pipe_"+name.s;
            mkfifo(pn.c_str(),0666);
            int fd=open(pn.c_str(),O_RDWR);
            if(fd<0)die("Cannot open pipe: "+name.s+pnet::sysErr(),n->ln);
            auto h=std::shared_ptr<PipeHandle>(new PipeHandle{(uintptr_t)fd,(uintptr_t)fd,name.s,false,true},[](PipeHandle*p){
                if(!p->closed){::close((int)p->rh);if(p->named)::unlink(("/tmp/sl_pipe_"+p->name).c_str());}delete p;});
#endif
            return Value::MkPipe(h);
        }
    }

    Value evalShmLink(NP n,std::shared_ptr<Env> env){
        auto name=eval(n->ch[0],env);
        if(name.ty!=Value::STR)die("shm name must be string",n->ln);
        size_t sz=4096;
        if((int)n->ch.size()>1){auto sv=eval(n->ch[1],env);if(sv.ty==Value::NUM){if(sv.n<0)die("shm size must be non-negative",n->ln);sz=(size_t)sv.n;}}
#ifdef _WIN32
        std::string mn="Local\\sl_shm_"+name.s;
        HANDLE hm=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,(DWORD)sz,mn.c_str());
        bool isOwner=(hm!=NULL&&GetLastError()!=ERROR_ALREADY_EXISTS);
        if(!hm){hm=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,mn.c_str());
            if(!hm)die("Cannot open shm: "+name.s+pnet::sysErr(),n->ln);}
        void*ptr=MapViewOfFile(hm,FILE_MAP_ALL_ACCESS,0,0,sz);
        if(!ptr){CloseHandle(hm);die("MapViewOfFile failed"+pnet::sysErr(),n->ln);}
        auto h=std::shared_ptr<ShmHandle>(new ShmHandle{ptr,sz,name.s,(uintptr_t)hm,false,isOwner},[](ShmHandle*p){
            if(!p->closed){UnmapViewOfFile(p->ptr);CloseHandle((HANDLE)p->hm);}delete p;});
#else
        std::string sn="/sl_shm_"+name.s;
        bool isOwner=false;
        int fd=shm_open(sn.c_str(),O_CREAT|O_EXCL|O_RDWR,0666);
        if(fd>=0){isOwner=true;ftruncate(fd,sz);}
        else if(errno==EEXIST){fd=shm_open(sn.c_str(),O_RDWR,0666);if(fd<0)die("shm_open failed"+pnet::sysErr(),n->ln);}
        else die("shm_open failed"+pnet::sysErr(),n->ln);
        void*ptr=mmap(NULL,sz,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        if(ptr==MAP_FAILED){::close(fd);die("mmap failed"+pnet::sysErr(),n->ln);}
        auto h=std::shared_ptr<ShmHandle>(new ShmHandle{ptr,sz,name.s,(uintptr_t)fd,false,isOwner},[sn](ShmHandle*p){
            if(!p->closed){munmap(p->ptr,p->sz);::close((int)p->hm);if(p->owner)shm_unlink(("/sl_shm_"+p->name).c_str());}delete p;});
#endif
        return Value::MkShm(h);
    }

    // ===== Builtins =====
    void regFn(const std::string&name,std::function<Value(std::vector<Value>&,std::shared_ptr<Env>,int)>fn){
        auto cl=std::make_shared<Closure>();cl->native=fn;G->set(name,Value::Fn(cl));
    }
    void regFn(const std::string&name,std::function<Value(std::vector<Value>&)>fn){
        regFn(name,[fn](std::vector<Value>&a,std::shared_ptr<Env>,int)->Value{return fn(a);});
    }

    std::string quoteCmdArg(const std::string&arg){
        if(arg.empty())return "\"\"";
        bool need=false;
        for(char c:arg)if(std::isspace((unsigned char)c)||c=='"'){need=true;break;}
        if(!need)return arg;
        std::string out="\"";
        int backslashes=0;
        for(char c:arg){
            if(c=='\\'){backslashes++;continue;}
            if(c=='"'){
                out.append(backslashes*2+1,'\\');
                out.push_back('"');
                backslashes=0;
                continue;
            }
            if(backslashes){out.append(backslashes,'\\');backslashes=0;}
            out.push_back(c);
        }
        if(backslashes)out.append(backslashes*2,'\\');
        out.push_back('"');
        return out;
    }

    std::shared_ptr<ProcHandle> spawnProc(const std::vector<std::string>&argv,const std::string&cwd,bool detached,int l){
        if(argv.empty())die("sys.spawn requires non-empty argv",l);
#ifdef _WIN32
        std::string cmd;
        for(size_t i=0;i<argv.size();i++){
            if(i)cmd.push_back(' ');
            cmd+=quoteCmdArg(argv[i]);
        }
        STARTUPINFOA si{};
        si.cb=sizeof(si);
        PROCESS_INFORMATION pi{};
        DWORD flags=detached?DETACHED_PROCESS:0;
        std::vector<char> buf(cmd.begin(),cmd.end());
        buf.push_back('\0');
        BOOL ok=CreateProcessA(
            NULL,
            buf.data(),
            NULL,
            NULL,
            FALSE,
            flags,
            NULL,
            cwd.empty()?NULL:cwd.c_str(),
            &si,
            &pi
        );
        if(!ok)die("sys.spawn failed"+pnet::sysErr(),l);
        CloseHandle(pi.hThread);
        return std::shared_ptr<ProcHandle>(
            new ProcHandle{(uintptr_t)pi.hProcess,(uint64_t)pi.dwProcessId,false,false,0},
            [](ProcHandle*p){
                if(!p->closed&&p->h)CloseHandle((HANDLE)p->h);
                delete p;
            }
        );
#else
        pid_t pid=fork();
        if(pid<0)die("sys.spawn failed"+pnet::sysErr(),l);
        if(pid==0){
            if(!cwd.empty())chdir(cwd.c_str());
            if(detached)setsid();
            std::vector<char*> cargs;
            for(auto&arg:argv)cargs.push_back(const_cast<char*>(arg.c_str()));
            cargs.push_back(nullptr);
            execvp(cargs[0],cargs.data());
            _exit(127);
        }
        return std::shared_ptr<ProcHandle>(new ProcHandle{0,(uint64_t)pid,false,false,0},[](ProcHandle*p){delete p;});
#endif
    }

    Value makeSysObj(){
        VMap sys;
        sys.push_back({"cwd",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(!a.empty())die("sys.cwd requires 0 args",l);
            return Value::Str(fs::current_path().string());
        })});
        sys.push_back({"chdir",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<1||a[0].ty!=Value::STR)die("sys.chdir requires string path",l);
            fs::current_path(fs::path(a[0].s));
            return Value::Null();
        })});
        sys.push_back({"exists",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<1||a[0].ty!=Value::STR)die("sys.exists requires string path",l);
            return Value::Bool(fs::exists(fs::path(a[0].s)));
        })});
        sys.push_back({"is_file",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<1||a[0].ty!=Value::STR)die("sys.is_file requires string path",l);
            return Value::Bool(fs::is_regular_file(fs::path(a[0].s)));
        })});
        sys.push_back({"is_dir",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<1||a[0].ty!=Value::STR)die("sys.is_dir requires string path",l);
            return Value::Bool(fs::is_directory(fs::path(a[0].s)));
        })});
        sys.push_back({"mkdir",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.empty()||a[0].ty!=Value::STR)die("sys.mkdir requires string path",l);
            bool rec=false;
            if(a.size()>1){
                if(a[1].ty!=Value::BOOL)die("sys.mkdir recursive must be bool",l);
                rec=a[1].n!=0;
            }
            if(rec)fs::create_directories(fs::path(a[0].s));
            else fs::create_directory(fs::path(a[0].s));
            return Value::Null();
        })});
        sys.push_back({"listdir",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            fs::path p=".";
            if(!a.empty()){
                if(a[0].ty!=Value::STR)die("sys.listdir requires string path",l);
                p=fs::path(a[0].s);
            }
            std::vector<std::string> names;
            for(auto&ent:fs::directory_iterator(p))names.push_back(ent.path().filename().string());
            std::sort(names.begin(),names.end());
            VVec out;for(auto&n:names)out.push_back(Value::Str(n));
            return Value::List(std::move(out));
        })});
        sys.push_back({"read_text",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.empty()||a[0].ty!=Value::STR)die("sys.read_text requires string path",l);
            std::ifstream f(fs::path(a[0].s),std::ios::binary);
            if(!f)die("sys.read_text cannot open "+a[0].s,l);
            return Value::Str(std::string((std::istreambuf_iterator<char>(f)),{}));
        })});
        sys.push_back({"write_text",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<2||a[0].ty!=Value::STR||a[1].ty!=Value::STR)die("sys.write_text requires (path,text)",l);
            std::ofstream f(fs::path(a[0].s),std::ios::binary|std::ios::trunc);
            if(!f)die("sys.write_text cannot open "+a[0].s,l);
            f<<a[1].s;
            if(!f)die("sys.write_text failed "+a[0].s,l);
            return Value::Null();
        })});
        sys.push_back({"append_text",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<2||a[0].ty!=Value::STR||a[1].ty!=Value::STR)die("sys.append_text requires (path,text)",l);
            std::ofstream f(fs::path(a[0].s),std::ios::binary|std::ios::app);
            if(!f)die("sys.append_text cannot open "+a[0].s,l);
            f<<a[1].s;
            if(!f)die("sys.append_text failed "+a[0].s,l);
            return Value::Null();
        })});
        sys.push_back({"remove",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.empty()||a[0].ty!=Value::STR)die("sys.remove requires string path",l);
            bool rec=false;
            if(a.size()>1){
                if(a[1].ty!=Value::BOOL)die("sys.remove recursive must be bool",l);
                rec=a[1].n!=0;
            }
            fs::path p(a[0].s);
            if(rec)return Value::Num((double)fs::remove_all(p));
            return Value::Bool(fs::remove(p));
        })});
        sys.push_back({"rename",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.size()<2||a[0].ty!=Value::STR||a[1].ty!=Value::STR)die("sys.rename requires (src,dst)",l);
            fs::rename(fs::path(a[0].s),fs::path(a[1].s));
            return Value::Null();
        })});
        sys.push_back({"sleep",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.empty()||a[0].ty!=Value::NUM)die("sys.sleep requires number seconds",l);
            if(a[0].n<0)die("sys.sleep requires non-negative seconds",l);
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(a[0].n*1000.0)));
            return Value::Null();
        })});
        sys.push_back({"spawn",mkNative([this](std::vector<Value>&a,std::shared_ptr<Env>,int l)->Value{
            if(a.empty())die("sys.spawn requires argv list",l);
            auto argv=needStrListArg(a[0],"sys.spawn requires argv list of strings",l);
            std::string cwd;
            bool detached=false;
            if(a.size()>1){
                if(a[1].ty!=Value::MAP)die("sys.spawn opts must be map",l);
                if(auto*v=mapFind(a[1],"cwd")){
                    if(v->ty!=Value::STR)die("sys.spawn opts.cwd must be string",l);
                    cwd=v->s;
                }
                if(auto*v=mapFind(a[1],"detached")){
                    if(v->ty!=Value::BOOL)die("sys.spawn opts.detached must be bool",l);
                    detached=v->n!=0;
                }
            }
            return Value::MkProc(spawnProc(argv,cwd,detached,l));
        })});
        return Value::Map(std::move(sys));
    }

    void initBuiltins(){
        regFn("print",[](std::vector<Value>&a)->Value{
            for(int i=0;i<(int)a.size();i++){if(i)std::cout<<" ";std::cout<<a[i].ToStr();}
            std::cout<<"\n";std::cout.flush();return Value::Null();});
        regFn("debug",[](std::vector<Value>&a)->Value{
            for(int i=0;i<(int)a.size();i++){
                if(i)std::cout<<" ";
                std::cout<<"debug("<<a[i].TyName()<<": "<<a[i].ToStr()<<")";
            }
            std::cout<<"\n";std::cout.flush();
            return a.empty()?Value::Null():a[0];
        });
        regFn("probe",[](std::vector<Value>&a)->Value{
            for(int i=0;i<(int)a.size();i++){
                if(i)std::cout<<" ";
                std::cout<<"probe("<<a[i].TyName()<<": "<<a[i].ToStr()<<")";
            }
            std::cout<<"\n";std::cout.flush();
            return a.empty()?Value::Null():a[0];
        });
        regFn("input",[](std::vector<Value>&)->Value{
            std::string l;std::getline(std::cin,l);return Value::Str(l);});
        regFn("len",[](std::vector<Value>&a)->Value{
            if(a.empty())die("len requires 1 arg");
            auto&v=a[0];
            if(v.ty==Value::LIST)return Value::Num((double)v.list().size());
            if(v.ty==Value::STR)return Value::Num((double)v.s.size());
            if(v.ty==Value::MAP)return Value::Num((double)v.map().size());
            die("len: bad type");return Value::Null();});
        regFn("type",[](std::vector<Value>&a)->Value{return Value::Str(a[0].TyName());});
        regFn("int",[](std::vector<Value>&a)->Value{
            if(a[0].ty==Value::NUM)return Value::Num((double)(int64_t)a[0].n);
            if(a[0].ty==Value::STR){try{return Value::Num((double)std::stoll(a[0].s));}
                catch(...){die("Cannot convert string to int: \""+a[0].s+"\"");}}
            die("int: bad type");return Value::Null();});
        regFn("float",[](std::vector<Value>&a)->Value{
            if(a[0].ty==Value::NUM)return Value::Num(a[0].n);
            if(a[0].ty==Value::STR){try{return Value::Num(std::stod(a[0].s));}
                catch(...){die("Cannot convert string to float: \""+a[0].s+"\"");}}
            die("float: bad type");return Value::Null();});
        regFn("str",[](std::vector<Value>&a)->Value{return Value::Str(a[0].ToStr());});
        regFn("range",[](std::vector<Value>&a)->Value{
            int start=0,end=0,step=1;
            if(a.size()==1){end=(int)a[0].n;}
            else if(a.size()==2){start=(int)a[0].n;end=(int)a[1].n;}
            else if(a.size()>=3){start=(int)a[0].n;end=(int)a[1].n;step=(int)a[2].n;}
            if(step==0)die("range: step cannot be 0");
            VVec r;
            if(step>0)for(int i=start;i<end;i+=step)r.push_back(Value::Num(i));
            else for(int i=start;i>end;i+=step)r.push_back(Value::Num(i));
            return Value::List(std::move(r));});
        regFn("abs",[](std::vector<Value>&a)->Value{return Value::Num(std::abs(a[0].n));});
        regFn("min",[](std::vector<Value>&a)->Value{return a[0].n<a[1].n?a[0]:a[1];});
        regFn("max",[](std::vector<Value>&a)->Value{return a[0].n>a[1].n?a[0]:a[1];});
        regFn("sort",[](std::vector<Value>&a)->Value{
            if(a[0].ty!=Value::LIST)die("sort requires list");
            VVec r=a[0].list();
            std::sort(r.begin(),r.end(),[](const Value&a,const Value&b){
                if(a.ty==Value::NUM&&b.ty==Value::NUM)return a.n<b.n;
                if(a.ty==Value::STR&&b.ty==Value::STR)return a.s<b.s;
                return false;});
            return Value::List(std::move(r));});
        regFn("error",[](std::vector<Value>&a)->Value{
            die(a.empty()?"error":a[0].ToStr());return Value::Null();});
        regFn("import",[this](std::vector<Value>&a,std::shared_ptr<Env> env,int l)->Value{
            if(a.empty()||a[0].ty!=Value::STR)die("import requires string path",l);
            return importModule(a[0].s,env,l);
        });
        // Pipe-friendly global wrappers for list methods
        regFn("map",[this](std::vector<Value>&a,std::shared_ptr<Env> env,int)->Value{
            if(a.size()<2||a[0].ty!=Value::LIST)die("map(list,fn)");
            VVec r;for(auto&el:a[0].list()){std::vector<Value>ar{el};r.push_back(callFn(a[1],ar,0,env));}
            return Value::List(std::move(r));});
        regFn("filter",[this](std::vector<Value>&a,std::shared_ptr<Env> env,int)->Value{
            if(a.size()<2||a[0].ty!=Value::LIST)die("filter(list,fn)");
            VVec r;for(auto&el:a[0].list()){std::vector<Value>ar{el};if(callFn(a[1],ar,0,env).IsTrue())r.push_back(el);}
            return Value::List(std::move(r));});
        regFn("reduce",[this](std::vector<Value>&a,std::shared_ptr<Env> env,int)->Value{
            if(a.size()<3||a[0].ty!=Value::LIST)die("reduce(list,fn,init)");
            Value acc=a[2];for(auto&el:a[0].list()){std::vector<Value>ar{acc,el};acc=callFn(a[1],ar,0,env);}return acc;});
        regFn("find",[this](std::vector<Value>&a,std::shared_ptr<Env> env,int)->Value{
            if(a.size()<2||a[0].ty!=Value::LIST)die("find(list,fn)");
            for(auto&el:a[0].list()){std::vector<Value>ar{el};if(callFn(a[1],ar,0,env).IsTrue())return el;}return Value::Null();});
        regFn("join",[](std::vector<Value>&a)->Value{
            if(a.size()<2||a[0].ty!=Value::LIST)die("join(list,sep)");
            std::string r;auto&l=a[0].list();for(size_t i=0;i<l.size();i++){if(i)r+=a[1].s;r+=l[i].ToStr();}
            return Value::Str(r);});
        regFn("reverse",[](std::vector<Value>&a)->Value{
            if(a[0].ty!=Value::LIST)die("reverse requires list");
            return Value::List(VVec(a[0].list().rbegin(),a[0].list().rend()));});
        regFn("push",[](std::vector<Value>&a)->Value{
            if(a[0].ty!=Value::LIST)die("push requires list");
            a[0].list().push_back(a[1]);return a[0];});

        // ===== pcall — protected call =====
        regFn("pcall",[this](std::vector<Value>&a,std::shared_ptr<Env> env,int)->Value{
            if(a.empty()||a[0].ty!=Value::FN)die("pcall requires function");
            try{
                VVec empty;
                auto r=callFn(a[0],empty,0,env);
                VMap m;m.push_back({"ok",Value::Bool(true)});m.push_back({"value",r});
                return Value::Map(std::move(m));
            }catch(const Err&e){
                VMap m;m.push_back({"ok",Value::Bool(false)});m.push_back({"error",Value::Str(e.what())});
                return Value::Map(std::move(m));
            }
        });

        // ===== tcp_listen — TCP server =====
        regFn("tcp_listen",[this](std::vector<Value>&a)->Value{
            if(a.empty()||a[0].ty!=Value::STR)die("tcp_listen requires string address");
            auto&s=a[0].s;auto colon=s.rfind(':');
            if(colon==std::string::npos)die("Invalid listen address: "+s);
            std::string host=s.substr(0,colon);int port=std::stoi(s.substr(colon+1));
            if(port<1||port>65535)die("Port out of range: "+std::to_string(port));
            int backlog=16;
            if(a.size()>1&&a[1].ty==Value::NUM)backlog=(int)a[1].n;
            if(backlog<0)die("tcp_listen backlog must be non-negative");

#ifdef _WIN32
            SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            if(sock==INVALID_SOCKET)die("socket() failed"+pnet::sysErr());
            int yes=1;setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(yes));
            struct sockaddr_in sa{};sa.sin_family=AF_INET;sa.sin_port=htons((u_short)port);
            if(host.empty()||host=="0.0.0.0")sa.sin_addr.s_addr=INADDR_ANY;
            else inet_pton(AF_INET,host.c_str(),&sa.sin_addr);
            if(::bind(sock,(struct sockaddr*)&sa,sizeof(sa))!=0){closesocket(sock);die("bind failed: "+s+pnet::sysErr());}
            if(::listen(sock,backlog)!=0){closesocket(sock);die("listen failed: "+s+pnet::sysErr());}
            auto h=std::shared_ptr<TcpServerHandle>(new TcpServerHandle{(uintptr_t)sock,false},[](TcpServerHandle*p){
                if(!p->closed){closesocket((SOCKET)p->fd);}delete p;});
#else
            int sock=socket(AF_INET,SOCK_STREAM,0);
            if(sock<0)die("socket() failed"+pnet::sysErr());
            int yes=1;setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
            struct sockaddr_in sa{};sa.sin_family=AF_INET;sa.sin_port=htons(port);
            if(host.empty()||host=="0.0.0.0")sa.sin_addr.s_addr=INADDR_ANY;
            else inet_pton(AF_INET,host.c_str(),&sa.sin_addr);
            if(::bind(sock,(struct sockaddr*)&sa,sizeof(sa))!=0){::close(sock);die("bind failed: "+s+pnet::sysErr());}
            if(::listen(sock,backlog)!=0){::close(sock);die("listen failed: "+s+pnet::sysErr());}
            auto h=std::shared_ptr<TcpServerHandle>(new TcpServerHandle{(uintptr_t)sock,false},[](TcpServerHandle*p){
                if(!p->closed){::close((int)p->fd);}delete p;});
#endif
            return Value::MkTcpS(h);
        });

        // ===== JSON builtins =====
        regFn("json_encode",[](std::vector<Value>&a)->Value{
            if(a.empty())die("json_encode requires 1 arg");
            return Value::Str(json::encode(a[0]));
        });
        regFn("json_parse",[](std::vector<Value>&a)->Value{
            if(a.empty()||a[0].ty!=Value::STR)die("json_parse requires string");
            size_t p=0;
            return json::parse(a[0].s,p);
        });
        G->set("sys",makeSysObj());
    }
};

} // namespace sl

