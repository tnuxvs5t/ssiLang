#pragma once
#include "types.h"

namespace sl {

struct Lexer{
    const std::string&src;
    int p=0,ln=1;
    std::vector<Tok> toks;

    Lexer(const std::string&s):src(s){
        if(src.size()>=3 &&
           (unsigned char)src[0]==0xEF &&
           (unsigned char)src[1]==0xBB &&
           (unsigned char)src[2]==0xBF){
            p=3;
        }
        scan();
    }
    char ch(){return p<(int)src.size()?src[p]:0;}
    char pk(int d=1){int i=p+d;return i<(int)src.size()?src[i]:0;}

    void scan(){
        while(p<(int)src.size()){
            while(p<(int)src.size()&&(ch()==' '||ch()=='\t'))p++;
            if(p>=(int)src.size())break;
            char c=ch();
            if(c=='#'){while(p<(int)src.size()&&ch()!='\n')p++;continue;}
            if(c=='\n'){ln++;p++;add(tNL,"\n");continue;}
            if(c=='\r'){p++;continue;}
            if(isdigit((unsigned char)c)){doNum();continue;}
            if(c=='"'){doStr();continue;}
            if(isalpha((unsigned char)c)||c=='_'){doId();continue;}
            // two-char
            if(c=='='&&pk()=='='){p+=2;add(tEQEQ,"==");continue;}
            if(c=='!'&&pk()=='='){p+=2;add(tNEQ,"!=");continue;}
            if(c=='<'&&pk()=='='){p+=2;add(tLE,"<=");continue;}
            if(c=='>'&&pk()=='='){p+=2;add(tGE,">=");continue;}
            if(c=='-'&&pk()=='>'){p+=2;add(tARROW,"->");continue;}
            if(c==':'&&pk()==':'){p+=2;add(tDCOL,"::");continue;}
            if(c=='|'&&pk()=='>'){p+=2;add(tPIPE,"|>");continue;}
            if(c=='='&&pk()=='>'){p+=2;add(tFAT,"=>");continue;}
            if(c=='|'&&pk()=='|'){p+=2;add(tDPIPE,"||");continue;}
            if(c=='@'&&pk()=='@'){p+=2;add(tATAT,"@@");continue;}
            if(c=='?'&&pk()=='.'){p+=2;add(tQDOT,"?.");continue;}
            if(c=='?'&&pk()=='['){p+=2;add(tQBR,"?[");continue;}
            // single-char
            Tk t=tEOF;
            switch(c){
            case '+':t=tPLUS;break;case '-':t=tMINUS;break;case '*':t=tSTAR;break;
            case '/':t=tSLASH;break;case '%':t=tPCT;break;case '=':t=tEQ;break;
            case '<':t=tLT;break;case '>':t=tGT;break;
            case '(':t=tLP;break;case ')':t=tRP;break;
            case '[':t=tLB;break;case ']':t=tRB;break;
            case '{':t=tLC;break;case '}':t=tRC;break;
            case '.':t=tDOT;break;case ',':t=tCOMMA;break;
            case ':':t=tCOLON;break;case '&':t=tAMP;break;
            default:die(std::string("Unexpected char '")+c+"'",ln);
            }
            p++;add(t,std::string(1,c));
        }
        add(tEOF,"");
    }

    void doNum(){
        int s=p;
        while(isdigit((unsigned char)ch()))p++;
        if(ch()=='.'&&isdigit((unsigned char)pk())){p++;while(isdigit((unsigned char)ch()))p++;}
        add(tNUM,src.substr(s,p-s));
    }
    void doStr(){
        p++;std::string r;
        while(ch()&&ch()!='"'){
            if(ch()=='\\'){p++;
                switch(ch()){case'n':r+='\n';break;case't':r+='\t';break;
                case'\\':r+='\\';break;case'"':r+='"';break;case'0':r+='\0';break;
                default:r+=ch();}
            }else{if(ch()=='\n')ln++;r+=ch();}
            p++;
        }
        if(ch()=='"')p++;
        add(tSTR,r);
    }
    void doId(){
        int s=p;
        while(isalnum((unsigned char)ch())||ch()=='_')p++;
        std::string w=src.substr(s,p-s);
        Tk t=tID;
        if(w=="fn")t=tFN;else if(w=="if")t=tIF;else if(w=="else")t=tELSE;
        else if(w=="while")t=tWHILE;else if(w=="for")t=tFOR;else if(w=="in")t=tIN;
        else if(w=="return")t=tRET;else if(w=="break")t=tBRK;else if(w=="continue")t=tCONT;
        else if(w=="and")t=tAND;else if(w=="or")t=tOR;else if(w=="not")t=tNOT;
        else if(w=="true")t=tTRUE;else if(w=="false")t=tFALSE;else if(w=="null")t=tNULL;
        add(t,w);
    }
    void add(Tk t,std::string v){toks.push_back({t,std::move(v),ln});}
};

} // namespace sl
