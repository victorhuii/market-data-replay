#include "replay.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char**argv){
    try{
        std::string input,load,save;bool summary=false;
        for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--summary")summary=true;else{replay::require(i+1<argc,"missing option value");std::string value=argv[++i];if(arg=="--input")input=value;else if(arg=="--load")load=value;else if(arg=="--save")save=value;else throw std::runtime_error("unknown option");}}
        replay::require(!input.empty(),"usage: replay --input capture.mdr [--load state.chk] [--save state.chk] [--summary]");replay::State state;
        if(!load.empty()){replay::require(std::filesystem::file_size(load)<=2000000,"checkpoint too large");std::ifstream file(load,std::ios::binary);replay::require(bool(file),"checkpoint open failed");std::string raw((std::istreambuf_iterator<char>(file)),{});state=replay::State::restore(raw);}
        replay::require(std::filesystem::file_size(input)<=64000000,"capture exceeds 64 MB limit");std::ifstream file(input,std::ios::binary);replay::require(bool(file),"capture open failed");size_t packets=0,messages=0,total_bytes=0;auto start=std::chrono::steady_clock::now();
        while(file.peek()!=EOF){uint8_t prefix[4];file.read(reinterpret_cast<char*>(prefix),4);replay::require(file.gcount()==4,"truncated capture prefix");auto length=replay::be(prefix,0,4);total_bytes+=4+static_cast<size_t>(length);replay::require(total_bytes<=64000000,"capture exceeds cumulative 64 MB limit");replay::require(length>=20 && length<=65507,"capture datagram length bounds");std::vector<uint8_t> bytes(static_cast<size_t>(length));file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(length));replay::require(static_cast<uint64_t>(file.gcount())==length,"truncated captured datagram");messages+=state.apply(bytes);++packets;replay::require(packets<=100000 && messages<=1000000,"capture replay count limit");}
        auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();replay::require(!state.session.empty(),"empty capture needs a checkpoint");
        if(!save.empty()){auto pending=save+".pending";std::ofstream out(pending,std::ios::binary|std::ios::trunc);out<<state.checkpoint();out.flush();replay::require(bool(out),"checkpoint write failed");out.close();std::filesystem::rename(pending,save);}
        std::cout<<std::setprecision(12)<<"{\"session_hex\":\""<<replay::hextext(state.session)<<"\",\"next_sequence\":"<<state.next<<",\"ended\":"<<(state.ended?"true":"false")<<",\"active_orders\":"<<state.orders.size()<<",\"packets\":"<<packets<<",\"new_messages\":"<<messages<<",\"elapsed_seconds\":"<<elapsed<<",\"messages_per_second\":"<<(elapsed>0?messages/elapsed:0)<<",\"state_fnv64\":\""<<replay::fnv(state.body())<<"\"";
        if(!summary){std::cout<<",\"orders\":[";bool first=true;for(const auto& [id,o]:state.orders){if(!first)std::cout<<',';first=false;std::cout<<"{\"id\":"<<id<<",\"locate\":"<<o.locate<<",\"side\":\""<<o.side<<"\",\"quantity\":"<<o.quantity<<",\"price\":"<<o.price<<",\"stock_hex\":\""<<replay::hextext(o.stock)<<"\",\"mpid_hex\":\""<<replay::hextext(o.mpid)<<"\"}";}std::cout<<']';}
        std::cout<<"}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
