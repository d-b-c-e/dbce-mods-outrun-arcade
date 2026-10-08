#include "../src/main/directx/cabinet_force.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <climits>
using namespace forcefeedback;
static int checks;
#define CHECK(v) do { ++checks; if (!(v)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v); std::exit(1); } } while(0)
struct Fake : CabinetSink {
    bool open_ok = true, send_ok = true, stop_ok = true;
    int opens = 0, stops = 0, releases = 0, hold = 0, fail_on = -1;
    std::vector<int> sent;
    bool open(const unsigned char*, int duration) override { ++opens; hold = duration; return open_ok; }
    bool send(int force) override { sent.push_back(force); return send_ok && static_cast<int>(sent.size()) != fail_on; }
    bool stop() override { ++stops; return stop_ok; }
    void release() override { ++releases; }
};
int main() {
    CabinetSettings defaults{9000,8500,20};
    unsigned char guid[16]{1};
    int output = 123;
    // Exact old nonzero arithmetic for all cabinet motor codes/settings.
    for (const auto limits : {defaults, CabinetSettings{32767,12287,50}, CabinetSettings{10000,0,500}})
        for (int cmd=1;cmd<=15;++cmd) {
            if(cmd==8) continue;
            const int step=cmd<8?cmd-1:15-cmd;
            const int old_magnitude=std::min(10000,limits.maximum-((limits.maximum-limits.minimum)/7)*step);
            CHECK(cabinet_force(cmd,step,limits,output));
            CHECK(output==(cmd<8?-old_magnitude:old_magnitude));
        }
    CHECK(cabinet_force(0,INT_MIN,defaults,output) && output==0);
    CHECK(cabinet_force(8,INT_MAX,defaults,output) && output==0);
    CHECK(!cabinet_force(16,0,defaults,output) && output==0);
    CHECK(!cabinet_force(1,-1,defaults,output));
    CHECK(!cabinet_force(1,8,defaults,output));
    CHECK(!cabinet_force(1,0,{INT_MAX,0,20},output));
    CHECK(!cabinet_force(1,0,{10,11,20},output));
    CHECK(!cabinet_force(1,0,{10,-1,20},output));
    {
        Fake f; CabinetForce c(f);
        unsigned char empty[16]{};
        CHECK(!c.initialize(nullptr,defaults)); CHECK(!c.initialize(empty,defaults)); CHECK(f.opens==0);
        CHECK(c.initialize(guid,defaults)); CHECK(f.hold==100);
        CHECK(f.sent==std::vector<int>({0,0}) && f.stops==1);
        CHECK(c.set(1,0) && f.sent.size()==2); // inactive until game gate
        CHECK(c.set_active(true)); CHECK(c.set(1,0));
        CHECK(f.sent==std::vector<int>({0,0,0,-9000}));
        CHECK(c.set(8,0) && f.sent.back()==0 && f.stops==2);
        CHECK(c.set(15,0)); CHECK(f.sent[f.sent.size()-2]==0 && f.sent.back()==9000);
        CHECK(c.set_active(false) && f.sent.back()==0 && f.stops==3);
        auto count=f.sent.size(); CHECK(c.set(1,0) && f.sent.size()==count);
        CHECK(c.set_active(true) && c.set(1,0));
        CHECK(f.sent[f.sent.size()-2]==0);
        c.close(); CHECK(f.releases==1 && !c.supported() && f.sent.back()==0);
        c.close(); CHECK(f.releases==1);
    }
    {
        Fake f; f.open_ok=false; CabinetForce c(f);
        CHECK(!c.initialize(guid,defaults)); CHECK(f.releases==1 && f.stops==1);
        CHECK(!c.initialize(guid,defaults) && f.opens==1);
    }
    {
        Fake f; f.fail_on=1; CabinetForce c(f);
        CHECK(!c.initialize(guid,defaults)); CHECK(f.sent.size()==2 && f.stops==1 && f.releases==1);
        CHECK(!c.supported());
    }
    {
        Fake f; CabinetForce c(f); CHECK(c.initialize(guid,defaults));
        CHECK(c.set_active(true)); f.fail_on=3;
        CHECK(!c.set(15,0)); CHECK(f.sent.back()==0 && f.releases==1);
        for(int v:f.sent) CHECK(v==0); // rejected neutral never followed by force
    }
    {
        Fake f; CabinetForce c(f); CHECK(c.initialize(guid,defaults));
        CHECK(c.set_active(true)); f.fail_on=4;
        CHECK(!c.set(15,0)); CHECK(f.sent.back()==0 && f.releases==1 && !c.supported());
        CHECK(!c.set(1,0)); CHECK(!c.initialize(guid,defaults) && f.opens==1);
    }
    for(bool zero_ok : {false,true}) for(bool stop_ok : {false,true}) {
        Fake f; CabinetForce c(f); CHECK(c.initialize(guid,defaults));
        CHECK(c.set_active(true) && c.set(15,0));
        f.send_ok=zero_ok; f.stop_ok=stop_ok;
        CHECK(c.set_active(false)==(zero_ok||stop_ok));
        CHECK(c.supported()==(zero_ok||stop_ok));
        CHECK(f.stops>=2); // failed zero never suppresses stop
        c.close(); CHECK(f.releases==1);
    }
    {
        Fake f; CabinetForce c(f); CHECK(c.initialize(guid,{9000,8500,INT_MAX})); CHECK(f.hold==500);
        CHECK(c.set_active(true) && c.set(1,0)); CHECK(!c.set(INT_MIN,0));
        CHECK(!c.supported() && f.releases==1 && f.sent.back()==0);
    }
    std::printf("PASS: %d cabinet mapping and production lifecycle checks; fake sink only\n", checks);
}
