#include <iostream>
#include <sstream>
#include <string>
#include "cabinet_producer_fixture.hpp"
int main(int argc,char** argv) {
    if(argc==2 && std::string(argv[1])=="--identity") {
        std::cout<<DBCE_CABINET_PRODUCER_SHA256<<" "<<DBCE_CABINET_MAPPER_SHA256<<"\n";return 0;
    }
    if(argc!=2){std::cerr<<"usage: cabinet_replay <durationSeconds>; numeric frame protocol on stdin\n";return 2;}
    char* tail=nullptr;const long seconds=std::strtol(argv[1],&tail,10);
    if(!tail || *tail || seconds<1 || seconds>120)return 2;
    using namespace cabinet_signal;
    Buffer buffer;OOutputs replay;replay.init();poison_unrecorded(replay);
    forcefeedback::CabinetSettings settings{};
    std::string line;size_t count=0;bool ended=false;
    while(std::getline(std::cin,line)) {
        if(line.size()>8192 || ended){std::cerr<<"oversize or trailing data\n";return 2;}
        std::istringstream stream(line);std::string kind;stream>>kind;
        if(kind=="end") {
            size_t expected=0;std::string extra;
            if(!(stream>>expected) || stream>>extra || expected!=count || !buffer.complete()){
                std::cerr<<"incomplete footer\n";return 2;
            }
            ended=true;continue;
        }
        if(kind!="frame" || count>=8192){std::cerr<<"record kind/count\n";return 2;}
        std::array<int64_t,46> values{};for(auto& v:values)if(!(stream>>v)){std::cerr<<"row numeric field\n";return 2;}
        std::string extra;if(stream>>extra){std::cerr<<"extra field\n";return 2;}
        if(!in(values[0],0,121000000) || !in(values[1],0,UINT32_MAX)){std::cerr<<"time/update range\n";return 2;}
        for(size_t n=11;n<values.size();++n)if(!in(values[n],INT32_MIN,INT32_MAX)){std::cerr<<"integer range\n";return 2;}
        Frame f{};size_t at=0;f.elapsed_us=uint64_t(values[at++]);f.update=uint32_t(values[at++]);
        for(auto& v:f.inputs)v=values[at++];for(auto& v:f.before)v=int32_t(values[at++]);for(auto& v:f.after)v=int32_t(values[at++]);
        f.command=int(values[at++]);f.step=int(values[at++]);f.nominal=int(values[at++]);
        f.delivery_result=int(values[at++]);
        forcefeedback::CabinetSettings current{int(values[at]),int(values[at+1]),int(values[at+2])};
        if(count==0){settings=current;if(!buffer.arm(settings,unsigned(seconds)))return 2;}
        if(current.maximum!=settings.maximum || current.minimum!=settings.minimum || current.hold_ms!=settings.hold_ms || !buffer.append(f)) {
            std::cerr<<"invalid/discontinuous frame "<<count<<"\n";return 2;
        }
        // Validate ranges and continuity before any original table lookup.
        if(count==0)restore(replay,f.before);
        const auto& v=f.inputs;
        Input input{int(v[Game]),int16_t(v[Crash]),int16_t(v[Skid]),int16_t(v[Curve]),int16_t(v[Steering]),
            int16_t(v[Motor]),int16_t(v[XDiff]),uint32_t(v[Increment]),uint8_t(v[Wheels])};
        auto actual=advance(replay,input,settings);
        if(actual.before!=f.before || actual.after!=f.after || actual.command!=f.command || actual.step!=f.step || actual.nominal!=f.nominal) {
            std::cerr<<"producer mismatch row "<<count<<"\n";return 1;
        }
        ++count;
    }
    if(!ended){std::cerr<<"missing footer\n";return 2;}
    std::cout<<"PASS "<<count<<" original cabinet rows; exact requests/state; no native sink\n";
}
