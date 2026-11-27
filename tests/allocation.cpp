#include "../src/replay.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
static bool injecting=false;
static size_t remaining=0;
void* operator new(std::size_t n){if(injecting && remaining--==0){injecting=false;throw std::bad_alloc();}void* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
int main(){
    std::vector<std::vector<uint8_t>> packets;
    for(int i=0;i<2;++i){uint8_t prefix[4];std::cin.read(reinterpret_cast<char*>(prefix),4);if(std::cin.gcount()!=4)return 2;auto n=replay::be(prefix,0,4);if(n>65507)return 3;std::vector<uint8_t> b(static_cast<size_t>(n));std::cin.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n));if(static_cast<uint64_t>(std::cin.gcount())!=n)return 4;packets.push_back(std::move(b));}
    replay::State expected;expected.apply(packets[0]);expected.apply(packets[1]);auto final_body=expected.body();size_t failures=0;bool reached_success=false;
    for(size_t nth=0;nth<1000;++nth){
        replay::State state;state.apply(packets[0]);auto before=state.body();remaining=nth;injecting=true;
        try{state.apply(packets[1]);injecting=false;replay::require(state.body()==final_body,"wrong committed state");reached_success=true;break;}
        catch(const std::bad_alloc&){injecting=false;if(state.body()!=before)return 5;++failures;}
    }
    if(!reached_success || failures<5)return 6;
    std::cout<<failures<<" allocation failure points preserved state\n";
}
