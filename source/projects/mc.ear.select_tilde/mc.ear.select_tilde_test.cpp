/// @file
/// @brief   Unit tests for mc.ear.select~ (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include "../shared/ear_max_test.h"
#include "mc.ear.select_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(mc_ear_select_tilde)

#include "mc.ear.select_tilde.cpp"

using namespace earmax_test;

namespace {

constexpr long k_block = 16;

struct mc_audio_io {
    std::vector<std::vector<double>> ins;
    std::vector<std::vector<double>> outs;
    std::vector<double*> in_ptrs;
    std::vector<double*> out_ptrs;

    mc_audio_io(size_t inputs, size_t outputs, long frames)
        : ins(inputs, std::vector<double>(frames, 0.0)), outs(outputs, std::vector<double>(frames, 0.0))
    {
        for (size_t i = 0; i < ins.size(); ++i) {
            std::fill(ins[i].begin(), ins[i].end(), static_cast<double>(i + 1));    // channel i carries the value i+1
            in_ptrs.push_back(ins[i].data());
        }
        for (auto& o : outs) {
            out_ptrs.push_back(o.data());
        }
    }

    audio_bundle input()
    {
        return audio_bundle{ in_ptrs.data(), static_cast<long>(ins.size()), static_cast<long>(ins[0].size()) };
    }

    audio_bundle output()
    {
        return audio_bundle{ out_ptrs.data(), static_cast<long>(outs.size()), static_cast<long>(outs[0].size()) };
    }
};

} // namespace

SCENARIO("mc.ear.select~ routes the selected channels of a multichannel signal") {
    ext_main(nullptr);

    GIVEN("an instance created with the channel numbers 7 and 8") {
        test_wrapper_args<mc_ear_select_tilde> an_instance(atoms{ 7, 8 });
        mc_ear_select_tilde& obj = an_instance;

        THEN("the selection is 0-based internally and sets the output channel count") {
            REQUIRE(obj.selected() == std::vector<long>{ 6, 7 });
            REQUIRE(obj.mc_output_channels(0) == 2);
        }

        WHEN("an 8-channel signal is processed") {
            mc_audio_io io(8, 2, k_block);
            obj(io.input(), io.output());
            THEN("the outputs carry input channels 7 and 8") {
                REQUIRE(io.outs[0][k_block - 1] == Approx(7.0));
                REQUIRE(io.outs[1][k_block - 1] == Approx(8.0));
            }
        }

        WHEN("the selection changes to a channel the input does not have") {
            obj.tracks(atoms{ 2, 20 });
            REQUIRE(obj.selected() == std::vector<long>{ 1, 19 });
            mc_audio_io io(8, 2, k_block);
            obj(io.input(), io.output());
            THEN("the missing channel is silent") {
                REQUIRE(io.outs[0][k_block - 1] == Approx(2.0));
                REQUIRE(io.outs[1][k_block - 1] == Approx(0.0).margin(1e-12));
            }
        }

        WHEN("the output has fewer channels than the selection (before the audio is restarted)") {
            obj.tracks(atoms{ 1, 2, 3 });
            mc_audio_io io(8, 2, k_block);
            obj(io.input(), io.output());
            THEN("the first selected channels are output") {
                REQUIRE(io.outs[0][k_block - 1] == Approx(1.0));
                REQUIRE(io.outs[1][k_block - 1] == Approx(2.0));
            }
        }

        WHEN("an invalid selection is sent") {
            obj.tracks(atoms{ 0 });
            obj.tracks(atoms{ symbol("one") });
            obj.tracks(atoms{});
            THEN("the previous selection stays") {
                REQUIRE(obj.selected() == std::vector<long>{ 6, 7 });
            }
        }
    }

    GIVEN("an instance without arguments") {
        test_wrapper<mc_ear_select_tilde> an_instance;
        mc_ear_select_tilde& obj = an_instance;
        THEN("it passes the first channel") {
            REQUIRE(obj.selected() == std::vector<long>{ 0 });
            REQUIRE(obj.mc_output_channels(0) == 1);
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_select_tilde, "mc.ear.select~")
