#include "../src/replay.hpp"
#include <iostream>
int main(){
    replay::State state;size_t accepted=0,rejected=0;
    for(;;){uint8_t prefix[4];std::cin.read(reinterpret_cast<char*>(prefix),4);if(std::cin.gcount()==0)break;if(std::cin.gcount()!=4)return 4;auto n=replay::be(prefix,0,4);if(n>65507)return 5;std::vector<uint8_t> b(static_cast<size_t>(n));std::cin.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n));if(static_cast<uint64_t>(std::cin.gcount())!=n)return 6;auto before=state.body();try{state.apply(b);++accepted;}catch(const std::exception&){++rejected;if(state.body()!=before)return 7;}}
    std::cout<<"{\"accepted\":"<<accepted<<",\"rejected\":"<<rejected<<",\"next_sequence\":"<<state.next<<",\"orders\":"<<state.orders.size()<<"}\n";
}
