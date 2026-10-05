/// @file
/// @brief   Unit tests for mc.ear.direct~ (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cmath>
#include <limits>

#include "../shared/ear_max_test.h"
#include "mc.ear.direct_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(mc_ear_direct_tilde)

#include "mc.ear.direct_tilde.cpp"

using namespace earmax_test;

namespace {

// 4+5+0 channel order: M+030 M-030 M+000 LFE1 M+110 M-110 U+030 U-030 U+110 U-110
constexpr size_t k_channels_450 = 10;
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

SCENARIO("mc.ear.direct~ renders a channel bed to a multichannel loudspeaker signal") {
    ext_main(nullptr);

    GIVEN("an instance rendering to 4+5+0") {
        test_wrapper_args<mc_ear_direct_tilde> an_instance(atoms{ symbol("4+5+0") });
        mc_ear_direct_tilde& obj = an_instance;

        REQUIRE(obj.channel_count() == k_channels_450);
        REQUIRE(obj.input_count() == 16);
        REQUIRE(obj.mc_output_channels(0) == static_cast<long>(k_channels_450));
        REQUIRE(obj.latency() == 0);

        obj.vector_size(k_block);
        obj.samplerate(48000.0);
        obj.ramp = 0.0;
        obj.dspsetup(atoms{ 48000.0, k_block });

        THEN("the output is delayed by the decorrelator compensation delay by default (align)") {
            REQUIRE(static_cast<bool>(obj.align));
            REQUIRE(obj.latency() == ear::decorrelatorCompensationDelay());
        }

        WHEN("an impulse is sent through an aligned bed channel") {
            obj.inputlayout(atoms{ symbol("0+5+0") });
            mc_audio_io io(1, k_channels_450, k_block);
            std::vector<double> front;
            for (long block = 0; block < 8; ++block) {
                std::fill(io.ins[0].begin(), io.ins[0].end(), 0.0);
                if (block == 0) {
                    io.ins[0][0] = 1.0;
                }
                obj(io.input(), io.output());
                front.insert(front.end(), io.outs[0].begin(), io.outs[0].end());
            }
            THEN("it arrives on M+030 exactly one latency later, like the direct path of the object renderers") {
                const auto latency = static_cast<size_t>(obj.latency());
                REQUIRE(latency == static_cast<size_t>(ear::decorrelatorCompensationDelay()));
                for (size_t n = 0; n < latency; ++n) {
                    REQUIRE(front[n] == Approx(0.0).margin(1e-12));
                }
                REQUIRE(front[latency] == Approx(1.0));
                REQUIRE(front[latency + 1] == Approx(0.0).margin(1e-12));
            }
        }

        WHEN("align is switched off and on again around a silent interval") {
            obj.inputlayout(atoms{ symbol("0+5+0") });
            mc_audio_io io(1, k_channels_450, k_block);
            io.ins[0][0] = 1.0;    // an impulse enters the delay line with align on
            obj(io.input(), io.output());
            obj.align = false;
            std::fill(io.ins[0].begin(), io.ins[0].end(), 0.0);
            double off_energy = 0.0;
            for (long block = 0; block < 8; ++block) {
                obj(io.input(), io.output());
                for (auto v : io.outs[0]) off_energy += v * v;
            }
            obj.align = true;
            double on_energy = 0.0;
            for (long block = 0; block < 8; ++block) {
                obj(io.input(), io.output());
                for (auto v : io.outs[0]) on_energy += v * v;
            }
            THEN("the bypassed interval is silent and the impulse is not replayed afterwards") {
                REQUIRE(off_energy == Approx(0.0).margin(1e-12));
                REQUIRE(on_energy == Approx(0.0).margin(1e-12));
            }
        }

        // the remaining scenarios look at the output within the first block
        obj.align = false;
        REQUIRE(obj.latency() == 0);

        WHEN("a 0+5+0 bed is declared with inputlayout") {
            obj.inputlayout(atoms{ symbol("0+5+0") });
            THEN("the first six channels carry the 0+5+0 labels and the LFE flag") {
                REQUIRE(obj.metadata(0).dstm.speakerLabels == std::vector<std::string>{ "M+030" });
                REQUIRE(obj.metadata(3).dstm.speakerLabels == std::vector<std::string>{ "LFE1" });
                REQUIRE(static_cast<bool>(obj.metadata(3).dstm.channelFrequency.lowPass));
                REQUIRE(*obj.metadata(3).dstm.channelFrequency.lowPass == Approx(120.0));
                REQUIRE(obj.metadata(6).dstm.speakerLabels.empty());
            }

            mc_audio_io io(6, k_channels_450, k_block);
            for (size_t i = 0; i < 6; ++i) {
                std::fill(io.ins[i].begin(), io.ins[i].end(), static_cast<double>(i + 1));
            }
            obj(io.input(), io.output());

            THEN("each bed channel maps onto the matching loudspeaker, with silent height channels") {
                for (size_t i = 0; i < 6; ++i) {
                    REQUIRE(io.outs[i][k_block - 1] == Approx(static_cast<double>(i + 1)));
                }
                for (size_t i = 6; i < k_channels_450; ++i) {
                    REQUIRE(io.outs[i][k_block - 1] == Approx(0.0).margin(1e-9));
                }
            }
        }

        WHEN("a 4+7+0 bed is rendered to 4+5+0 with the mapping rules") {
            obj.inputlayout(atoms{ symbol("4+7+0") });
            obj.anything(atoms{ symbol("packformat"), symbol("AP_00010017") });
            THEN("M+135 is routed to M+110 by the common definitions mapping") {
                // 4+7+0 channel order: M+030 M-030 M+000 LFE1 M+090 M-090 M+135 M-135 U+045 U-045 U+135 U-135
                const auto& g = obj.gains(6);
                REQUIRE(g[4] == Approx(1.0));    // M+110 in 4+5+0
            }
        }

        WHEN("channels are labelled with applyvalues and one is repositioned with setvalue") {
            obj.applyvalues(atoms{ symbol("speakerlabel"), symbol("M+000"), symbol("M-030") });
            obj.setvalue(atoms{ 3, symbol("position"), 30.0, 0.0 });
            THEN("labels win, and the unlabelled channel is placed by position") {
                REQUIRE(obj.gains(0)[2] == Approx(1.0));
                REQUIRE(obj.gains(1)[1] == Approx(1.0));
                REQUIRE(obj.gains(2)[0] == Approx(1.0));
            }
        }

        WHEN("bed channels are given in Cartesian coordinates, as a Dolby Atmos master places them") {
            obj.setvalue(atoms{ 1, symbol("cartesian"), 1 });
            obj.setvalue(atoms{ 1, symbol("position"), -1.0, 1.0, 0.0 });    // L
            obj.setvalue(atoms{ 2, symbol("cartesian"), 1 });
            obj.setvalue(atoms{ 2, symbol("position"), -1.0, 0.0, 0.0 });    // Lss: no side loudspeaker in 4+5+0
            obj.setvalue(atoms{ 3, symbol("cartesian"), 1 });
            obj.setvalue(atoms{ 3, symbol("position"), -1.0, 1.0, -1.0 });    // LFE
            obj.setvalue(atoms{ 3, symbol("lfe"), 1 });
            THEN("a corner of the cube lands on the loudspeaker at that allocentric position, the rest is panned") {
                REQUIRE(obj.gains(0)[0] == Approx(1.0));    // M+030
                const auto& side = obj.gains(1);
                REQUIRE(side[0] > 0.1);    // between M+030 ...
                REQUIRE(side[4] > 0.1);    // ... and M+110
                REQUIRE(obj.gains(2)[3] == Approx(1.0));    // LFE1
            }
            THEN("a position with two values is refused, and the bounds take six") {
                obj.setvalue(atoms{ 1, symbol("position"), 30.0, 0.0 });
                REQUIRE(obj.metadata(0).cartesian_position.X == Approx(-1.0));
                obj.setvalue(atoms{ 1, symbol("bounds"), 0.0, 10.0, 0.0, 10.0 });
                REQUIRE(obj.metadata(0).bounds.empty());
                obj.setvalue(atoms{ 1, symbol("bounds"), -1.0, -0.5, 0.5, 1.0, -0.5, 0.5 });
                REQUIRE(obj.metadata(0).bounds.size() == 6);
                obj.setvalue(atoms{ 1, symbol("cartesian"), 0 });
                REQUIRE(obj.metadata(0).bounds.empty());    // the bounds belong to the other coordinate system
            }
        }

        WHEN("non-finite bounds or positions are sent") {
            obj.setvalue(atoms{ 1, symbol("bounds"), 0.0, std::numeric_limits<double>::infinity(), 0.0, 0.0 });
            obj.setvalue(atoms{ 1, symbol("azimuth"), std::numeric_limits<double>::quiet_NaN() });
            THEN("they are rejected") {
                REQUIRE(obj.metadata(0).bounds.empty());
                REQUIRE(obj.metadata(0).position.azimuth == Approx(0.0));
            }
        }

        WHEN("an unknown input layout is requested") {
            obj.inputlayout(atoms{ symbol("nope") });
            THEN("nothing changes") {
                REQUIRE(obj.metadata(0).dstm.speakerLabels.empty());
            }
        }
    }
}


SCENARIO("mc.ear.direct~ grows its allocation when a message or the input addresses more channels") {
    ext_main(nullptr);

    GIVEN("an instance with the default 16 channels") {
        test_wrapper_args<mc_ear_direct_tilde> an_instance(atoms{ symbol("4+5+0") });
        mc_ear_direct_tilde& obj = an_instance;
        REQUIRE(obj.input_count() == 16);

        WHEN("setvalue addresses channel 30, as a Dolby master with three beds does") {
            obj.setvalue(atoms{ 30, symbol("speakerlabel"), symbol("M+000") });
            THEN("30 channels are allocated, chans follows and the label is set") {
                REQUIRE(obj.input_count() == 30);
                REQUIRE(static_cast<int>(obj.chans) == 30);
                REQUIRE(obj.metadata(29).dstm.speakerLabels == std::vector<std::string>{ "M+000" });
                REQUIRE(obj.gains(29).size() == k_channels_450);
            }
        }

        WHEN("the input signal carries 24 channels and the audio is restarted") {
            REQUIRE(obj.mc_input_changed(0, 24) == 0);
            obj.setvalue(atoms{ 24, symbol("speakerlabel"), symbol("M+030") });
            obj.vector_size(k_block);
            obj.samplerate(48000.0);
            obj.ramp = 0.0;
            obj.align = false;
            obj.dspsetup(atoms{ 48000.0, k_block });
            mc_audio_io io(24, k_channels_450, k_block);
            std::fill(io.ins[23].begin(), io.ins[23].end(), 1.0);
            obj(io.input(), io.output());
            THEN("24 channels are allocated and the new channel is rendered") {
                REQUIRE(obj.input_count() == 24);
                REQUIRE(static_cast<int>(obj.chans) == 24);
                REQUIRE(io.outs[0][k_block - 1] == Approx(1.0));    // M+030 of 4+5+0
                REQUIRE(io.outs[2][k_block - 1] == Approx(0.0).margin(1e-12));
            }
        }

        WHEN("inputlayout names 9+10+3") {
            obj.inputlayout(atoms{ symbol("9+10+3") });
            THEN("the 24 channels of the layout are allocated and labelled") {
                REQUIRE(obj.input_count() == 24);
                REQUIRE(obj.metadata(23).dstm.speakerLabels.size() == 1);
            }
        }

        WHEN("applyvalues gives 20 values") {
            atoms labels{ symbol("speakerlabel") };
            for (int i = 0; i < 20; ++i) {
                labels.push_back(symbol("M+000"));
            }
            obj.applyvalues(labels);
            THEN("20 channels are allocated and the last one is labelled") {
                REQUIRE(obj.input_count() == 20);
                REQUIRE(obj.metadata(19).dstm.speakerLabels == std::vector<std::string>{ "M+000" });
            }
        }

        WHEN("setvalue addresses a channel beyond the cap, a fraction of one, or no number at all") {
            obj.setvalue(atoms{ 2000, symbol("lfe"), 1 });
            obj.setvalue(atoms{ 1024.5, symbol("lfe"), 1 });
            obj.setvalue(atoms{ 20.5, symbol("lfe"), 1 });
            obj.setvalue(atoms{ std::numeric_limits<double>::infinity(), symbol("lfe"), 1 });
            THEN("nothing is allocated") {
                REQUIRE(obj.input_count() == 16);
            }
        }

        WHEN("chans is set lower") {
            obj.chans = 8;
            THEN("the allocation shrinks: only growth is automatic") {
                REQUIRE(obj.input_count() == 8);
            }
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_direct_tilde, "mc.ear.direct~")
