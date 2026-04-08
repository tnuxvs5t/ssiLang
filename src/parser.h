#pragma once
#include "lexer.h"

namespace sl {

struct Parser{
    const std::vector<Tok>&tk;
    int p=0;
    int depth=0; // bracket nesting depth for newline suppression
    int phGen=0;
    Parser(const std::vector<Tok>&t):tk(t){}

    int ln(){return tk[p].ln;}
    Tk peek(){return tk[p].t;}
    Tk pkAt(int d){return tk[p+d].t;}
    Tok eat(){return tk[p++];}
    bool match(Tk t){if(peek()==t){p++;return true;}return false;}
    Tok expect(Tk t){
        if(peek()!=t)die("Expected "+tokName(t)+" got '"+tk[p].v+"'",ln());
        return eat();
    }
    void skipNL(){while(peek()==tNL)p++;}

    static std::string tokName(Tk t){
        const char*n[]={"num","str","id","true","false","null","fn","if","else",
            "while","for","in","return","break","continue","and","or","not",
            "+","-","*","/","%","=","==","!=","<","<=",">",">=",
            "(",")","[","]","{","}",".",".?","?[",",",":",
            "->","|>","=>","||","@","$","&","newline","eof"};
        return n[t];
    }

    // ===== Program =====
    std::vector<NP> program(){
        std::vector<NP> r;
        skipNL();
        while(peek()!=tEOF){r.push_back(stmt());skipNL();}
        return r;
    }

    NP stmt(){
        skipNL();
        if(looksLikeCtxBlock())return ctxStmt();
        switch(peek()){
        case tFN:return fnStmt();case tIF:return ifStmt();
        case tWHILE:return whileStmt();case tFOR:return forStmt();
        case tRET:return retStmt();
        case tBRK:{int l=ln();eat();return Node::mk(Node::sBRK,l);}
        case tCONT:{int l=ln();eat();return Node::mk(Node::sCONT,l);}
        default:return bindOrExpr();
        }
    }

    bool looksLikeCtxBlock(){
        if(peek()!=tAT||pkAt(1)!=tLP)return false;
        int saveP=p,saveD=depth;
        try{
            eat();expect(tLP);depth++;skipNL();
            rawExpr();
            skipNL();expect(tRP);depth--;
            bool ok=peek()==tLC;
            p=saveP;depth=saveD;
            return ok;
        }catch(...){
            p=saveP;depth=saveD;
            return false;
        }
    }

    NP ctxStmt(){
        int l=ln();
        expect(tAT);expect(tLP);depth++;skipNL();
        auto ctx=rawExpr();
        skipNL();expect(tRP);depth--;
        auto body=block();
        auto n=Node::mk(Node::sCTX,l);n->ch={ctx,body};return n;
    }

    NP fnStmt(){
        int l=ln();eat();
        std::string name=expect(tID).v;
        expect(tLP);skipNL();
        std::vector<std::string> ps;
        if(peek()==tID){ps.push_back(eat().v);while(match(tCOMMA)){skipNL();ps.push_back(expect(tID).v);}}
        skipNL();expect(tRP);
        NP body;
        if(match(tFAT))body=wrapRet(expr());
        else body=block();
        auto n=Node::mk(Node::sFN,l);n->s=name;n->params=std::move(ps);n->ch.push_back(body);
        return n;
    }

    // Arrow body: => expr | => break | => continue | => return [expr]
    NP arrowBody(){
        if(peek()==tBRK){int l=ln();eat();return Node::mk(Node::sBRK,l);}
        if(peek()==tCONT){int l=ln();eat();return Node::mk(Node::sCONT,l);}
        if(peek()==tRET)return retStmt();
        return wrapExpr(expr());
    }

    NP ifStmt(){
        int l=ln();eat();
        auto cond=pipeExpr(); // no bare split in if-condition
        NP th,el;
        if(match(tFAT))th=arrowBody();else th=block();
        skipNL();
        if(match(tELSE)){
            skipNL();
            if(peek()==tIF)el=ifStmt();
            else if(match(tFAT))el=arrowBody();
            else el=block();
        }
        auto n=Node::mk(Node::sIF,l);n->ch.push_back(cond);n->ch.push_back(th);
        if(el)n->ch.push_back(el);
        return n;
    }

    NP whileStmt(){int l=ln();eat();auto c=rawExpr();auto b=block();
        auto n=Node::mk(Node::sWHILE,l);n->ch={c,b};return n;}
    NP forStmt(){int l=ln();eat();std::string v=expect(tID).v;expect(tIN);
        auto it=rawExpr();auto b=block();auto n=Node::mk(Node::sFOR,l);n->s=v;n->ch={it,b};return n;}
    NP retStmt(){int l=ln();eat();NP v;
        if(peek()!=tNL&&peek()!=tEOF&&peek()!=tRC)v=expr();
        auto n=Node::mk(Node::sRET,l);if(v)n->ch.push_back(v);return n;}

    NP block(){expect(tLC);skipNL();auto n=Node::mk(Node::sBLOCK,ln());
        while(peek()!=tRC&&peek()!=tEOF){n->ch.push_back(stmt());skipNL();}
        expect(tRC);return n;}

    NP wrapRet(NP e){auto r=Node::mk(Node::sRET,e->ln);r->ch.push_back(e);return r;}
    NP wrapExpr(NP e){auto r=Node::mk(Node::sEXPR,e->ln);r->ch.push_back(e);return r;}

    NP bindOrExpr(){
        auto e=rawExpr();
        if(peek()==tEQ){
            int l=ln();eat();auto val=expr();
            if(e->k==Node::eID){auto n=Node::mk(Node::sBIND,l);n->s=e->s;n->ch.push_back(val);return n;}
            if(validLv(e)){auto n=Node::mk(Node::sASGN,l);n->ch={e,val};return n;}
            die("Invalid assignment target",l);
        }
        e=normalizeExpr(e);
        auto n=Node::mk(Node::sEXPR,e->ln);n->ch.push_back(e);return n;
    }

    bool validLv(NP e){
        if(e->k==Node::eOUTER)return true;
        if(e->k==Node::eMEM||e->k==Node::eIDX)
            return e->ch[0]->k==Node::eID||e->ch[0]->k==Node::eCTX||e->ch[0]->k==Node::eOUTER||validLv(e->ch[0]);
        return false;
    }

    // ===== Expressions =====
    NP rawExpr(){skipNL();return split();}
    NP expr(){return normalizeExpr(rawExpr());}

    NP split(){
        auto l=pipeExpr();
        if(peek()==tFAT){eat();auto th=expr();expect(tDPIPE);auto el=split();
            auto n=Node::mk(Node::eSPLIT,l->ln);n->ch={l,th,el};return n;}
        return l;
    }

    NP pipeExpr(){
        auto l=orExpr();
        for(;;){
            skipNL();
            if(peek()!=tPIPE)break;
            eat();
            skipNL();
            auto[tgt,args]=flowTarget();
            auto c=Node::mk(Node::eCALL,l->ln);
            c->ch.push_back(tgt);
            c->ch.push_back(l);
            for(auto&a:args)c->ch.push_back(a);
            l=c;
        }
        return l;
    }

    std::pair<NP,std::vector<NP>> flowTarget(){
        NP tgt;
        if(peek()==tPCT)tgt=derivExpr();
        else if(peek()==tAMP)tgt=linkExpr();
        else{tgt=Node::mk(Node::eID,ln());tgt->s=expect(tID).v;}
        std::vector<NP> args;
        if(peek()==tLP){eat();skipNL();
            if(peek()!=tRP){args.push_back(rawExpr());while(match(tCOMMA)){skipNL();args.push_back(rawExpr());}}
            skipNL();expect(tRP);}
        return{tgt,args};
    }

    NP orExpr(){auto l=andExpr();while(peek()==tOR){auto o=eat().t;auto r=andExpr();
        auto n=Node::mk(Node::eBIN,l->ln);n->op=o;n->ch={l,r};l=n;}return l;}
    NP andExpr(){auto l=eqExpr();while(peek()==tAND){auto o=eat().t;auto r=eqExpr();
        auto n=Node::mk(Node::eBIN,l->ln);n->op=o;n->ch={l,r};l=n;}return l;}
    NP eqExpr(){auto l=cmpExpr();while(peek()==tEQEQ||peek()==tNEQ){auto o=eat().t;auto r=cmpExpr();
        auto n=Node::mk(Node::eBIN,l->ln);n->op=o;n->ch={l,r};l=n;}return l;}
    NP cmpExpr(){auto l=addExpr();while(peek()==tLT||peek()==tLE||peek()==tGT||peek()==tGE){
        auto o=eat().t;auto r=addExpr();auto n=Node::mk(Node::eBIN,l->ln);n->op=o;n->ch={l,r};l=n;}return l;}
    NP addExpr(){auto l=mulExpr();while(peek()==tPLUS||peek()==tMINUS){auto o=eat().t;auto r=mulExpr();
        auto n=Node::mk(Node::eBIN,l->ln);n->op=o;n->ch={l,r};l=n;}return l;}
    NP mulExpr(){auto l=unaryE();while(peek()==tSTAR||peek()==tSLASH||peek()==tPCT){
        auto o=eat().t;auto r=unaryE();auto n=Node::mk(Node::eBIN,l->ln);n->op=o;n->ch={l,r};l=n;}return l;}

    NP unaryE(){
        if(peek()==tMINUS||peek()==tNOT){int l=ln();auto o=eat().t;auto e=unaryE();
            auto n=Node::mk(Node::eUN,l);n->op=o;n->ch.push_back(e);return n;}
        if(peek()==tPCT)return derivExpr();
        if(peek()==tAMP)return linkExpr();
        return postfix();
    }

    NP derivExpr(){
        int l=ln();eat();std::string m=expect(tID).v;
        if(m!="copy"&&m!="deep"&&m!="snap"&&m!="net")die("Unknown derive mode: "+m,l);
        auto e=unaryE();auto n=Node::mk(Node::eDERIV,l);n->s=m;n->ch.push_back(e);return n;
    }
    NP linkExpr(){
        int l=ln();eat();std::string k=expect(tID).v;
        if(k!="ref"&&k!="tcp"&&k!="pipe"&&k!="shm")die("Unknown link kind: "+k,l);
        auto n=Node::mk(Node::eLINK,l);n->s=k;
        if(k=="ref"){n->ch.push_back(unaryE());}
        else{expect(tLP);skipNL();
            if(peek()!=tRP){n->ch.push_back(rawExpr());while(match(tCOMMA)){skipNL();n->ch.push_back(rawExpr());}}
            skipNL();expect(tRP);}
        return n;
    }

    NP postfix(){
        auto l=atom();
        for(;;){
            if(depth>0)skipNL(); // inside brackets, newlines are insignificant
            if(peek()==tDOT){eat();auto n=Node::mk(Node::eMEM,l->ln);n->s=expect(tID).v;n->ch.push_back(l);l=n;}
            else if(peek()==tQDOT){eat();auto n=Node::mk(Node::eSMEM,l->ln);n->s=expect(tID).v;n->ch.push_back(l);l=n;}
            else if(peek()==tLB){eat();depth++;skipNL();auto i=rawExpr();skipNL();expect(tRB);depth--;
                auto n=Node::mk(Node::eIDX,l->ln);n->ch={l,i};l=n;}
            else if(peek()==tQBR){eat();depth++;skipNL();auto i=rawExpr();skipNL();expect(tRB);depth--;
                auto n=Node::mk(Node::eSIDX,l->ln);n->ch={l,i};l=n;}
            else if(peek()==tLP){eat();depth++;skipNL();auto c=Node::mk(Node::eCALL,l->ln);c->ch.push_back(l);
                if(peek()!=tRP){c->ch.push_back(rawExpr());while(match(tCOMMA)){skipNL();c->ch.push_back(rawExpr());}}
                skipNL();expect(tRP);depth--;l=c;}
            else break;
        }
        return l;
    }

    bool isLamStart(){
        int q=p;
        auto sk=[&]{while(tk[q].t==tNL)q++;};
        if(tk[q].t!=tLP)return false;
        q++;
        sk();
        if(tk[q].t==tRP){q++;return tk[q].t==tARROW;}
        if(tk[q].t!=tID)return false;
        q++;
        sk();
        while(tk[q].t==tCOMMA){
            q++;
            sk();
            if(tk[q].t!=tID)return false;
            q++;
            sk();
        }
        if(tk[q].t!=tRP)return false;
        q++;
        return tk[q].t==tARROW;
    }

    NP atom(){
        int l=ln();
        if(peek()==tNUM){auto v=eat();auto n=Node::mk(Node::eNUM,l);n->num=std::stod(v.v);return n;}
        if(peek()==tSTR){auto v=eat();auto n=Node::mk(Node::eSTR,l);n->s=v.v;return n;}
        if(peek()==tTRUE){eat();auto n=Node::mk(Node::eBOOL,l);n->num=1;return n;}
        if(peek()==tFALSE){eat();auto n=Node::mk(Node::eBOOL,l);n->num=0;return n;}
        if(peek()==tNULL){eat();return Node::mk(Node::eNUL,l);}
        if(peek()==tDOLLAR){
            auto v=eat();
            auto n=Node::mk(Node::ePH,l);
            n->num=v.v.size()==1?1:std::stod(v.v.substr(1));
            return n;
        }
        if(peek()==tAT){
            eat();
            if(peek()==tID){auto n=Node::mk(Node::eOUTER,l);n->s=eat().v;return n;}
            if(peek()==tLP){
                eat();depth++;skipNL();
                auto ctx=rawExpr();
                skipNL();expect(tRP);depth--;
                expect(tFAT);
                auto body=expr();
                auto n=Node::mk(Node::eCTXEXPR,l);n->ch={ctx,body};return n;
            }
            return Node::mk(Node::eCTX,l);
        }

        if(peek()==tID){
            if(pkAt(1)==tARROW){auto nm=eat().v;eat();auto b=expr();
                auto n=Node::mk(Node::eLAM,l);n->params.push_back(nm);n->ch.push_back(wrapRet(b));return n;}
            auto n=Node::mk(Node::eID,l);n->s=eat().v;return n;
        }

        if(peek()==tLP){
            if(isLamStart()){eat();depth++;skipNL();
                std::vector<std::string> ps;
                if(peek()==tID){
                    ps.push_back(expect(tID).v);
                    while(match(tCOMMA)){skipNL();ps.push_back(expect(tID).v);}
                }
                skipNL();expect(tRP);depth--;expect(tARROW);auto b=expr();
                auto n=Node::mk(Node::eLAM,l);n->params=std::move(ps);n->ch.push_back(wrapRet(b));return n;}
            eat();depth++;skipNL();auto e=rawExpr();skipNL();expect(tRP);depth--;return e;
        }

        if(peek()==tLB){eat();depth++;skipNL();auto n=Node::mk(Node::eLIST,l);
            if(peek()!=tRB){n->ch.push_back(rawExpr());while(match(tCOMMA)){skipNL();n->ch.push_back(rawExpr());}}
            skipNL();expect(tRB);depth--;return n;}

        if(peek()==tLC){eat();depth++;skipNL();auto n=Node::mk(Node::eMAP,l);
            if(peek()!=tRC){auto[k,v]=mapEnt();n->entries.push_back({k,v});
                while(match(tCOMMA)){skipNL();auto[k2,v2]=mapEnt();n->entries.push_back({k2,v2});}}
            skipNL();expect(tRC);depth--;return n;}

        die("Unexpected '"+tk[p].v+"'",l);
        return nullptr;
    }

    std::pair<std::string,NP> mapEnt(){
        skipNL();std::string key;
        if(peek()==tSTR)key=eat().v;
        else if(peek()==tID)key=eat().v;
        else die("Expected map key",ln());
        expect(tCOLON);skipNL();return{key,rawExpr()};
    }

    NP normalizeExpr(NP n){
        n=normalizeRaw(n);
        if(containsPlaceholder(n))return wrapPlaceholderLambda(n);
        return n;
    }

    NP normalizeRaw(NP n){
        if(!n)return n;
        switch(n->k){
        case Node::eUN:
        case Node::eDERIV:
        case Node::eLINK:
            for(auto&i:n->ch)i=normalizeRaw(i);
            break;
        case Node::eBIN:
        case Node::eMEM:
        case Node::eSMEM:
        case Node::eIDX:
        case Node::eSIDX:
        case Node::eSPLIT:
        case Node::eLIST:
            for(auto&i:n->ch)i=normalizeRaw(i);
            break;
        case Node::eCTXEXPR:
            if(!n->ch.empty())n->ch[0]=normalizeRaw(n->ch[0]);
            if(n->ch.size()>1)n->ch[1]=normalizeExpr(n->ch[1]);
            break;
        case Node::eMAP:
            for(auto&ent:n->entries)ent.second=normalizeRaw(ent.second);
            break;
        case Node::eCALL:{
            if(!n->ch.empty())n->ch[0]=normalizeRaw(n->ch[0]);
            for(size_t i=1;i<n->ch.size();i++)n->ch[i]=normalizeRaw(n->ch[i]);
            normalizeHofCallArgs(n);
            break;
        }
        default:break;
        }
        return n;
    }

    bool containsPlaceholder(NP n){
        if(!n)return false;
        if(n->k==Node::ePH)return true;
        if(n->k==Node::eLAM)return false;
        for(auto&c:n->ch)if(containsPlaceholder(c))return true;
        for(auto&ent:n->entries)if(containsPlaceholder(ent.second))return true;
        return false;
    }

    int maxPlaceholder(NP n){
        if(!n)return 0;
        if(n->k==Node::ePH)return (int)n->num;
        if(n->k==Node::eLAM)return 0;
        int r=0;
        for(auto&c:n->ch)r=std::max(r,maxPlaceholder(c));
        for(auto&ent:n->entries)r=std::max(r,maxPlaceholder(ent.second));
        return r;
    }

    void replacePlaceholderNodes(NP&n,const std::vector<std::string>&params){
        if(!n||n->k==Node::eLAM)return;
        if(n->k==Node::ePH){
            int idx=(int)n->num;
            if(idx<1||idx>(int)params.size())die("Invalid placeholder index",n->ln);
            auto repl=Node::mk(Node::eID,n->ln);
            repl->s=params[idx-1];
            n=repl;
            return;
        }
        for(auto&c:n->ch)replacePlaceholderNodes(c,params);
        for(auto&ent:n->entries)replacePlaceholderNodes(ent.second,params);
    }

    NP wrapPlaceholderLambda(NP n){
        int argc=maxPlaceholder(n);
        if(argc<=0)return n;
        std::vector<std::string> params;
        int id=++phGen;
        for(int i=1;i<=argc;i++)params.push_back("__ph_auto_"+std::to_string(id)+"_"+std::to_string(i));
        replacePlaceholderNodes(n,params);
        auto lam=Node::mk(Node::eLAM,n->ln);
        lam->params=std::move(params);
        lam->ch.push_back(wrapRet(n));
        return lam;
    }

    bool isGlobalHof(const std::string&name){
        return name=="map"||name=="filter"||name=="find"||name=="reduce"||name=="pcall";
    }

    bool isMethodHof(const std::string&name){
        return name=="map"||name=="filter"||name=="find"||name=="reduce";
    }

    void normalizeHofArgAt(NP n,int idx){
        if(idx<(int)n->ch.size())n->ch[idx]=normalizeExpr(n->ch[idx]);
    }

    void normalizeHofCallArgs(NP n){
        if(n->k!=Node::eCALL||n->ch.empty())return;
        auto callee=n->ch[0];
        if(callee->k==Node::eID&&isGlobalHof(callee->s)){
            if(callee->s=="pcall")normalizeHofArgAt(n,1);
            else normalizeHofArgAt(n,2);
            return;
        }
        if((callee->k==Node::eMEM||callee->k==Node::eSMEM)&&isMethodHof(callee->s)){
            normalizeHofArgAt(n,1);
        }
    }

};

} // namespace sl
