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

EARMAX_TEST_GENERATE_MAXREF(mc_ear_direct_tilde, "mc.ear.direct~")
