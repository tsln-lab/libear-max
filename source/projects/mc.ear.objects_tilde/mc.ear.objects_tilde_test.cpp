/// @file
/// @brief   Unit tests for mc.ear.objects~ (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cmath>

#include "../shared/ear_max_test.h"
#include "mc.ear.objects_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(mc_ear_objects_tilde)

#include "mc.ear.objects_tilde.cpp"

using namespace earmax_test;

namespace {

// 0+5+0 channel order in libear: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr int k_m030 = 0;
constexpr int k_m_030 = 1;
constexpr int k_m000 = 2;
constexpr size_t k_channels_050 = 6;
constexpr long k_block = 64;

struct mc_audio_io {
    std::vector<std::vector<double>> ins;
    std::vector<std::vector<double>> outs;
    std::vector<double*> in_ptrs;
    std::vector<double*> out_ptrs;

    mc_audio_io(size_t inputs, size_t outputs, long frames)
        : ins(inputs, std::vector<double>(frames, 0.0)), outs(outputs, std::vector<double>(frames, 0.0))
    {
        for (auto& i : ins) in_ptrs.push_back(i.data());
        for (auto& o : outs) out_ptrs.push_back(o.data());
    }

    audio_bundle input()
    {
        return audio_bundle{ in_ptrs.data(), static_cast<long>(ins.size()), static_cast<long>(ins[0].size()) };
    }

    audio_bundle output()
    {
        return audio_bundle{ out_ptrs.data(), static_cast<long>(outs.size()), static_cast<long>(ins[0].size()) };
    }
};

} // namespace

SCENARIO("mc.ear.objects~ renders several objects to a multichannel loudspeaker signal") {
    ext_main(nullptr);

    GIVEN("an instance with the 0+5+0 layout and the default capacity") {
        test_wrapper_args<mc_ear_objects_tilde> an_instance(atoms{ symbol("0+5+0") });
        mc_ear_objects_tilde& obj = an_instance;

        REQUIRE(obj.channel_count() == k_channels_050);
        REQUIRE(obj.object_count() == 16);
        REQUIRE(obj.mc_output_channels(0) == static_cast<long>(k_channels_050));

        obj.vector_size(k_block);
        obj.samplerate(48000.0);
        obj.decorrelate = false;
        obj.ramp = 0.0;
        obj.dspsetup(atoms{ 48000.0, k_block });

        WHEN("three objects are placed with applyvalues and rendered") {
            obj.applyvalues(atoms{ symbol("azimuth"), 30.0, 0.0, -30.0 });
            THEN("each object's metadata follows") {
                REQUIRE(obj.metadata(0).azimuth == Approx(30.0));
                REQUIRE(obj.metadata(1).azimuth == Approx(0.0));
                REQUIRE(obj.metadata(2).azimuth == Approx(-30.0));
                REQUIRE(obj.metadata(3).azimuth == Approx(0.0));
            }

            mc_audio_io io(3, k_channels_050, k_block);
            std::fill(io.ins[0].begin(), io.ins[0].end(), 1.0);
            std::fill(io.ins[1].begin(), io.ins[1].end(), 0.5);
            std::fill(io.ins[2].begin(), io.ins[2].end(), 0.25);
            obj(io.input(), io.output());

            THEN("each object lands on its own loudspeaker in the output") {
                REQUIRE(io.outs[k_m030][k_block - 1] == Approx(1.0));
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(0.5));
                REQUIRE(io.outs[k_m_030][k_block - 1] == Approx(0.25));
                REQUIRE(io.outs[3][k_block - 1] == Approx(0.0).margin(1e-9));
            }
        }

        WHEN("setvalue addresses one object and plain messages address all") {
            obj.setvalue(atoms{ 2, symbol("azimuth"), 30.0 });
            obj.anything(atoms{ symbol("diffuse"), 0.5 });
            THEN("only object 2 moved, but every object is half diffuse") {
                REQUIRE(obj.metadata(1).azimuth == Approx(30.0));
                REQUIRE(obj.metadata(0).azimuth == Approx(0.0));
                REQUIRE(obj.metadata(0).otm.diffuse == Approx(0.5));
                REQUIRE(obj.metadata(15).otm.diffuse == Approx(0.5));
                REQUIRE(obj.direct_gains(1)[k_m030] == Approx(std::sqrt(0.5)));
            }
        }

        WHEN("setvalue 0 sets all objects and a position list sets all positions") {
            obj.setvalue(atoms{ 0, symbol("gain"), 0.5 });
            obj.list(atoms{ -30.0, 0.0 });
            THEN("all objects have the new gain and position") {
                for (size_t i = 0; i < obj.object_count(); ++i) {
                    REQUIRE(obj.metadata(i).otm.gain == Approx(0.5));
                    REQUIRE(obj.metadata(i).azimuth == Approx(-30.0));
                }
                REQUIRE(obj.direct_gains(0)[k_m_030] == Approx(0.5));
            }
        }

        WHEN("an out-of-range object index or an unknown parameter is used") {
            obj.setvalue(atoms{ 17, symbol("azimuth"), 30.0 });
            obj.setvalue(atoms{ 1, symbol("nonsense"), 30.0 });
            THEN("nothing changes") {
                for (size_t i = 0; i < obj.object_count(); ++i) {
                    REQUIRE(obj.metadata(i).azimuth == Approx(0.0));
                }
            }
        }

        WHEN("more input channels than objects are connected") {
            REQUIRE(obj.mc_input_changed(0, 20) == 0);
            mc_audio_io io(20, k_channels_050, k_block);
            for (auto& in : io.ins) std::fill(in.begin(), in.end(), 1.0);
            obj(io.input(), io.output());
            THEN("only the allocated objects are rendered") {
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(16.0));
            }
        }

        WHEN("decorrelation is on and one object is fully diffuse") {
            obj.decorrelate = true;
            obj.setvalue(atoms{ 1, symbol("diffuse"), 1.0 });
            REQUIRE(obj.latency() == ear::decorrelatorCompensationDelay());

            mc_audio_io io(2, k_channels_050, k_block);
            double energy_front = 0.0;
            std::vector<double> direct_front;
            for (long block = 0; block < 16; ++block) {
                for (auto& in : io.ins) std::fill(in.begin(), in.end(), 0.0);
                if (block == 0) {
                    io.ins[0][0] = 1.0;    // diffuse object
                    io.ins[1][0] = 1.0;    // direct object
                }
                obj(io.input(), io.output());
                for (auto v : io.outs[k_m000]) {
                    energy_front += v * v;
                }
                direct_front.insert(direct_front.end(), io.outs[k_m000].begin(), io.outs[k_m000].end());
            }
            THEN("the energy of both impulses arrives on M+000") {
                // one decorrelated impulse (unit energy) plus one delayed impulse (unit energy)
                REQUIRE(energy_front == Approx(2.0).epsilon(0.1));
                REQUIRE(std::abs(direct_front[obj.latency()]) > 0.5);
            }
        }
    }

    GIVEN("an instance with a custom capacity") {
        test_wrapper_args<mc_ear_objects_tilde> an_instance(atoms{ symbol("4+5+0") });
        mc_ear_objects_tilde& obj = an_instance;
        obj.chans = 4;
        THEN("four objects are allocated with the layout's channel count") {
            REQUIRE(obj.object_count() == 4);
            REQUIRE(obj.channel_count() == 10);
            REQUIRE(obj.direct_gains(3).size() == 10);
        }
    }
}
