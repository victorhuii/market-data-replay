#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_set>

namespace replay {
using Bytes=std::span<const uint8_t>;
inline void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
inline uint64_t be(Bytes b,size_t offset,size_t width){require(width<=8 && offset<=b.size() && width<=b.size()-offset,"truncated integer");uint64_t value=0;for(size_t i=0;i<width;++i)value=(value<<8)|b[offset+i];return value;}
inline std::string hex(Bytes b){const char* digits="0123456789abcdef";std::string out;for(auto v:b){out+=digits[v>>4];out+=digits[v&15];}return out;}
inline std::string text(Bytes b){return std::string(reinterpret_cast<const char*>(b.data()),b.size());}
inline bool alpha(const std::string& s){return !s.empty() && std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ' || c=='.' || c=='-';});}
inline bool printable(const std::string& s){return std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=32 && c<=126;});}
inline std::string hextext(const std::string& s){return hex(Bytes(reinterpret_cast<const uint8_t*>(s.data()),s.size()));}
inline std::string unhex(const std::string& s,size_t bytes){require(s.size()==bytes*2,"invalid hex length");std::string out;for(size_t i=0;i<s.size();i+=2){unsigned value=0;auto [end,ec]=std::from_chars(s.data()+i,s.data()+i+2,value,16);require(ec==std::errc{} && end==s.data()+i+2,"invalid hex");out+=static_cast<char>(value);}return out;}
inline uint64_t number(const std::string& s){uint64_t value=0;auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),value);require(!s.empty() && ec==std::errc{} && end==s.data()+s.size(),"invalid unsigned integer");return value;}
inline std::string token(std::istream& in){std::string s;require(bool(in>>s),"truncated checkpoint");return s;}
inline uint64_t number(std::istream& in){return number(token(in));}
inline uint64_t fnv(const std::string& s){uint64_t hash=14695981039346656037ULL;for(unsigned char c:s){hash^=c;hash*=1099511628211ULL;}return hash;}
struct Order {uint16_t locate;char side;uint32_t quantity,price;std::string stock,mpid;};
struct State {
    static constexpr size_t max_orders=10000;
    std::string session;uint64_t next=1,max_timestamp=0;bool ended=false;std::map<uint64_t,Order> orders;
    void event(Bytes b){
        require(b.size()>=11,"truncated ITCH header");char type=static_cast<char>(b[0]);size_t length=0;
        switch(type){case 'A':length=36;break;case 'F':length=40;break;case 'E':length=31;break;case 'C':length=36;break;case 'X':length=23;break;case 'D':length=19;break;case 'U':length=35;break;case 'S':length=12;break;default:throw std::runtime_error("unsupported ITCH type");}
        require(b.size()==length,"invalid ITCH length");auto timestamp=be(b,5,6);require(timestamp<86400000000000ULL,"invalid nanoseconds since midnight");max_timestamp=std::max(max_timestamp,timestamp);
        uint64_t locate=be(b,1,2);
        if(type=='S'){require(std::string("OSQMEC").find(static_cast<char>(b[11]))!=std::string::npos,"invalid system event code");return;}
        require(locate>0,"zero stock locate");uint64_t id=be(b,11,8);require(id>0,"zero order reference");
        if(type=='A'||type=='F'){
            require(!orders.contains(id),"duplicate active order reference");require(orders.size()<max_orders,"active order limit");char side=static_cast<char>(b[19]);uint64_t quantity=be(b,20,4),price=be(b,32,4);std::string stock=text(b.subspan(24,8)),mpid=type=='F'?text(b.subspan(36,4)):"    ";
            require((side=='B'||side=='S') && quantity>0 && price>0 && price<=2000000000ULL && alpha(stock) && stock.find_first_not_of(' ')!=std::string::npos && alpha(mpid),"invalid add fields");
            // A locate must continue identifying the same stock while active.
            for(const auto& [existing_id,order]:orders){(void)existing_id;require(order.locate!=locate || order.stock==stock,"locate stock mismatch");}
            orders.emplace(id,Order{static_cast<uint16_t>(locate),side,static_cast<uint32_t>(quantity),static_cast<uint32_t>(price),stock,mpid});return;
        }
        auto found=orders.find(id);require(found!=orders.end(),"unknown order reference");require(found->second.locate==locate,"order locate mismatch");
        if(type=='D'){orders.erase(found);return;}
        if(type=='U'){
            auto new_id=be(b,19,8),quantity=be(b,27,4),price=be(b,31,4);require(new_id>0 && new_id!=id && !orders.contains(new_id) && quantity>0 && price>0 && price<=2000000000ULL,"invalid replacement");
            Order replacement=found->second;replacement.quantity=static_cast<uint32_t>(quantity);replacement.price=static_cast<uint32_t>(price);orders.erase(found);orders.emplace(new_id,replacement);return;
        }
        auto quantity=be(b,19,4);require(quantity>0 && quantity<=found->second.quantity,"invalid reduction quantity");
        if(type=='C')require((b[31]=='Y'||b[31]=='N') && be(b,32,4)>0 && be(b,32,4)<=2000000000ULL,"invalid priced execution");
        found->second.quantity-=static_cast<uint32_t>(quantity);if(found->second.quantity==0)orders.erase(found);
    }
    // Packet commit is atomic. The optional copy implementation is retained as
    // a correctness/performance baseline; default journaling touches only IDs used.
    size_t apply(Bytes packet){
        require(packet.size()>=20 && packet.size()<=65507,"invalid MoldUDP64 datagram size");auto incoming=text(packet.first(10));require(printable(incoming),"invalid session bytes");auto sequence=be(packet,10,8),count=be(packet,18,2);require(sequence>0,"zero sequence");
        require(session.empty() || incoming==session,"session mismatch");
        if(count==0 || count==65535){
            require(packet.size()==20,"control packet trailing bytes");require(sequence==next,"control sequence mismatch");if(ended && count==65535)return 0;require(!ended,"session ended");State staged=*this;staged.session=incoming;staged.ended=count==65535;*this=std::move(staged);return 0;
        }
        require(sequence<=std::numeric_limits<uint64_t>::max()-count,"sequence overflow");auto end_sequence=sequence+count;
        std::vector<Bytes> events;events.reserve(static_cast<size_t>(count));size_t offset=20;
        for(uint64_t i=0;i<count;++i){auto size=be(packet,offset,2);offset+=2;require(size>0 && size<=packet.size()-offset,"invalid message block");events.push_back(packet.subspan(offset,static_cast<size_t>(size)));offset+=static_cast<size_t>(size);}
        require(offset==packet.size(),"datagram trailing bytes");
        if(end_sequence<=next)return 0; // Well-framed complete retransmission.
        require(!ended,"session ended");require(sequence<=next,"forward sequence gap");
#ifdef REPLAY_COPY_ATOMICITY
        State staged=*this;staged.session=incoming;size_t applied=0;
        for(size_t i=static_cast<size_t>(next-sequence);i<events.size();++i){staged.event(events[i]);++applied;}
        staged.next=end_sequence;*this=std::move(staged);return applied;
#else
        using Node=decltype(orders)::node_type;
        struct Undo {uint64_t id;Node original;};
        std::vector<Undo> journal;journal.reserve(events.size()*2);
        std::unordered_set<uint64_t> touched;touched.reserve(events.size()*2);
        auto previous_timestamp=max_timestamp;
        auto touch=[&](uint64_t id){
            if(!touched.insert(id).second)return;
            auto found=orders.find(id);
            if(found==orders.end()){journal.push_back(Undo{id,Node{}});return;}
            // Allocate the working copy before extracting the original node. The
            // original node remains owned by the journal, so rollback can restore
            // deleted/replaced orders without allocating, even after bad_alloc.
            decltype(orders) working;working.emplace(id,found->second);
            auto original=orders.extract(found);journal.push_back(Undo{id,std::move(original)});
            orders.insert(working.extract(id));
        };
        size_t applied=0;
        try{
            for(size_t i=static_cast<size_t>(next-sequence);i<events.size();++i){
                auto message=events[i];
                if(message.size()>=19 && std::string("AFECXDU").find(static_cast<char>(message[0]))!=std::string::npos){touch(be(message,11,8));if(message[0]=='U' && message.size()>=27)touch(be(message,19,8));}
                event(message);++applied;
            }
        }catch(...){
            for(auto& undo:journal)orders.erase(undo.id);
            for(auto& undo:journal){if(!undo.original.empty())orders.insert(std::move(undo.original));}
            max_timestamp=previous_timestamp;throw;
        }
        session.swap(incoming);next=end_sequence;return applied;
#endif
    }
    std::string body()const{
        std::ostringstream out;out<<"MDR1\n"<<hextext(session)<<' '<<next<<' '<<ended<<' '<<max_timestamp<<' '<<orders.size()<<'\n';
        for(const auto& [id,o]:orders){
            out<<id<<' '<<o.locate<<' '<<o.side<<' '<<o.quantity<<' '<<o.price<<' '<<hextext(o.stock)<<' '<<hextext(o.mpid)<<'\n';
        }
        return out.str();
    }
    std::string checkpoint()const{require(session.size()==10,"checkpoint requires an initialized session");auto data=body();return data+std::to_string(fnv(data))+"\n";}
    static State restore(const std::string& serialized){
        require(serialized.size()<=2000000 && !serialized.empty() && serialized.back()=='\n',"invalid checkpoint size/termination");auto boundary=serialized.rfind('\n',serialized.size()-2);require(boundary!=std::string::npos,"invalid checkpoint format");auto data=serialized.substr(0,boundary+1);require(number(serialized.substr(boundary+1,serialized.size()-boundary-2))==fnv(data),"checkpoint checksum mismatch");
        std::istringstream in(data);require(token(in)=="MDR1","checkpoint version");State result;result.session=unhex(token(in),10);require(printable(result.session),"checkpoint session");result.next=number(in);auto ended_value=number(in);result.max_timestamp=number(in);auto count=number(in);require(result.next>0 && ended_value<=1 && result.max_timestamp<86400000000000ULL && count<=max_orders,"checkpoint metadata bounds");result.ended=ended_value==1;
        std::map<uint16_t,std::string> symbols;
        for(uint64_t i=0;i<count;++i){auto id=number(in),locate=number(in);auto side=token(in);auto quantity=number(in),price=number(in);auto stock=unhex(token(in),8),mpid=unhex(token(in),4);require(id>0 && !result.orders.contains(id) && locate>0 && locate<=65535 && (side=="B"||side=="S") && quantity>0 && quantity<=4294967295ULL && price>0 && price<=2000000000ULL && alpha(stock) && stock.find_first_not_of(' ')!=std::string::npos && alpha(mpid),"checkpoint order bounds");
            require(!symbols.contains(static_cast<uint16_t>(locate)) || symbols[static_cast<uint16_t>(locate)]==stock,"checkpoint locate mismatch");symbols[static_cast<uint16_t>(locate)]=stock;
            result.orders.emplace(id,Order{static_cast<uint16_t>(locate),side[0],static_cast<uint32_t>(quantity),static_cast<uint32_t>(price),stock,mpid});}
        in>>std::ws;require(in.eof(),"checkpoint trailing fields");require(result.body()==data,"noncanonical checkpoint");return result;
    }
};
}
