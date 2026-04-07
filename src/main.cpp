#include "interp.h"

int main(int argc,char**argv){
    if(argc<2){
        sl::Interp interp;
        std::string line;
        while(std::cout<<"sl> "&&std::getline(std::cin,line)){
            if(line.empty())continue;
            try{
                sl::Lexer lex(line);
                sl::Parser par(lex.toks);
                auto prog=par.program();
                interp.run(prog);
            }catch(const sl::Err&e){std::cerr<<e.what()<<"\n";}
        }
        return 0;
    }
    try{
        sl::Interp interp;
        interp.runFile(argv[1]);
    }catch(const sl::Err&e){std::cerr<<e.what()<<"\n";return 1;}
    return 0;
}
