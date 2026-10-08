#include "cabinet_producer_fixture.hpp"
int main() {
    OOutputs m;m.init();
    // Independently readable fixed cases: neutral, stationary signs, original
    // seven-step mapping, then the exact eight-step crash waveform.
    Input i{12,0,0,0,0,128,0,0,0};
    auto r=advance(m,i);CHECK(r.command==0 && r.nominal==0);
    r=advance(m,i);CHECK(r.command==8 && r.nominal==0);
    i.motor=72;r=advance(m,i);CHECK(r.command==4 && r.step==3 && r.nominal==-8787);
    // At low speed with was_small_change=false the game retains its previous
    // command for this tick; preserve that behavior rather than invent symmetry.
    i.motor=184;r=advance(m,i);CHECK(r.command==4 && r.nominal==-8787);
    i.game=1;r=advance(m,i);CHECK(r.command==12 && r.step==3 && r.nominal==8787);
    m.init();i.game=12;i.motor=128;i.increment=200u<<16;i.crash=1;
    const int crash[]={8,8,2,2,8,8,14,14};
    for(int code:crash){r=advance(m,i);CHECK(r.command==code);}

    // Capture one original stateful trajectory, then replay from its first
    // checkpoint and from an arbitrary midstream checkpoint. No reset per row.
    std::vector<Row> tape;uint32_t random=0x14b0cafe;
    auto next=[&](){random=random*1664525u+1013904223u;return random;};
    m.init();
    for(int n=0;n<12000;++n){
        Input v{};v.game=n%191<170?12:1;
        v.crash=n%103<13?1:0;v.skid=n%83<9?-20:0;
        v.curve=static_cast<int16_t>(next()%150);
        v.motor=static_cast<int16_t>(72+next()%113);
        v.steering=static_cast<int16_t>(std::clamp((static_cast<int>(v.motor)-128)*256/112,-127,127));
        v.increment=(next()%300u)<<16;v.wheels=static_cast<uint8_t>(next()%4);
        v.x_diff=static_cast<int16_t>(static_cast<int>(next()%3)-1);
        tape.push_back(advance(m,v));
    }
    for(size_t start:{size_t(0),size_t(7311)}){
        OOutputs replay;replay.init();restore(replay,tape[start].before);
        poison_unrecorded(replay);
        for(size_t n=start;n<tape.size();++n)equal(tape[n],advance(replay,tape[n].input));
    }
    // Show that checkpoint/history is required: a midstream reset is not
    // interchangeable with restoring the original producer state.
    OOutputs reset;reset.init();CHECK(snapshot(reset)!=tape[7311].before);
    OOutputs disabled;disabled.init();CabinetReplayAccess::enabled(disabled,false);
    r=advance(disabled,i);CHECK(r.command==0 && r.nominal==0);
    {
        using namespace cabinet_signal;
        Buffer buffer;CHECK(buffer.arm({9000,8500,20},60));
        OOutputs live;live.init();std::vector<Frame> rows;
        for(uint32_t n=0;n<1801;++n){
            Input v{12,static_cast<int16_t>(n%37<8),0,static_cast<int16_t>(n%130),
                static_cast<int16_t>(static_cast<int>(n%255)-127),
                static_cast<int16_t>(72+n%113),static_cast<int16_t>(static_cast<int>(n%3)-1),
                (n%290)<<16,static_cast<uint8_t>(n%4)};
            auto f=frame(advance(live,v),uint64_t(n)*33334,900+n);
            CHECK(buffer.append(f));rows.push_back(f);
        }
        CHECK(buffer.complete());CHECK(buffer.frames().size()==1801);CHECK(!buffer.active());
        CHECK(!buffer.append(rows.back()));CHECK(buffer.frames().size()==1801);
        // Truncation, invalid values, dropped updates and mutated checkpoint
        // histories must never acquire a completion claim.
        for(int mutation=0;mutation<11;++mutation){
            Buffer bad;CHECK(bad.arm({9000,8500,20},60));CHECK(bad.append(rows[0]));
            Frame next=rows[1];
            switch(mutation){
                case 0:next.inputs[Motor]=0;break;
                case 1:next.inputs[Game]=1;break;
                case 2:next.nominal++;break;
                case 3:next.before[12]++;break;
                case 4:next.update++;break;
                case 5:next.elapsed_us=0;break;
                case 6:next.elapsed_us=1000001;break;
                case 7:next.after[1]=2;break;
                case 8:next.step++;break;
                case 9:next.before[4]=1;break; // cabinet-only movement state
                case 10:next.inputs[Steering]=128;break;
            }
            CHECK(!bad.append(next));CHECK(!bad.complete());CHECK(bad.frames().size()==1);
        }
        Buffer stopped;CHECK(stopped.arm({9000,8500,20},60));CHECK(stopped.append(rows[0]));
        stopped.stop(End::Duration);CHECK(!stopped.complete());CHECK(stopped.reason()==End::Interrupted);
        Buffer full;CHECK(full.arm({9000,8500,20},60,2));CHECK(full.append(rows[0]));CHECK(full.append(rows[1]));
        CHECK(!full.append(rows[2]));CHECK(full.reason()==End::Overflow && !full.complete());
        Buffer invalid;CHECK(!invalid.arm({9000,9500,20},60));CHECK(!invalid.arm({9000,8500,20},121));
        CHECK(!invalid.arm({9000,8500,20},60,8193));
    }
    std::printf("PASS %u original cabinet producer checks; 12000 synthetic rows, two stateful replays; no native calls\n",checks);
}
