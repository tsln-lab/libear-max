/// @file
/// @brief   Unit tests for mc.ear.decorrelate~ (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cmath>
#include <iostream>
#include <sstream>

#include "../shared/ear_max_test.h"
#include "mc.ear.decorrelate_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(mc_ear_decorrelate_tilde)

#include "mc.ear.decorrelate_tilde.cpp"

using namespace earmax_test;

namespace {

/// Collects what the object posts to the console as errors or warnings
/// (the Min logger writes them to std::cerr) while it is in scope.
struct stderr_capture {
    std::ostringstream text;
    std::streambuf* previous;
    stderr_capture() : previous(std::cerr.rdbuf(text.rdbuf())) {}
    ~stderr_capture() { std::cerr.rdbuf(previous); }
};

// 0+5+0 channel order: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr size_t k_channels_050 = 6;
constexpr size_t k_m000 = 2;
constexpr size_t k_lfe = 3;
constexpr long k_block = 64;
constexpr size_t k_taps = 512;
constexpr size_t k_blocks = k_taps / k_block;    // vectors that cover a whole filter

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

void start_audio(mc_ear_decorrelate_tilde& obj)
{
    obj.vector_size(k_block);
    obj.samplerate(48000.0);
    obj.dspsetup(atoms{ 48000.0, k_block });
}

/// The response of every output channel to a unit impulse on input channel
/// `channel` at sample 0, over enough vectors to cover a whole filter.
std::vector<std::vector<double>> impulse_response(mc_ear_decorrelate_tilde& obj, size_t inputs, size_t channel)
{
    const size_t outputs = obj.channel_count();
    std::vector<std::vector<double>> response(outputs);
    mc_audio_io io(inputs, outputs, k_block);
    for (size_t block = 0; block < k_blocks; ++block) {
        for (auto& in : io.ins) std::fill(in.begin(), in.end(), 0.0);
        if (block == 0) {
            io.ins[channel][0] = 1.0;
        }
        obj(io.input(), io.output());
        for (size_t ch = 0; ch < outputs; ++ch) {
            response[ch].insert(response[ch].end(), io.outs[ch].begin(), io.outs[ch].end());
        }
    }
    return response;
}

double energy(const std::vector<double>& samples)
{
    double sum = 0.0;
    for (double v : samples) sum += v * v;
    return sum;
}

double largest_difference(const std::vector<double>& a, const std::vector<float>& b)
{
    double worst = 0.0;
    for (size_t n = 0; n < std::min(a.size(), b.size()); ++n) {
        worst = std::max(worst, std::abs(a[n] - static_cast<double>(b[n])));
    }
    return worst;
}

} // namespace

SCENARIO("mc.ear.decorrelate~ applies the EAR's decorrelation filter of each loudspeaker") {
    ext_main(nullptr);

    GIVEN("an instance seeded by the 0+5+0 layout, with the audio started") {
        test_wrapper_args<mc_ear_decorrelate_tilde> an_instance(atoms{ symbol("0+5+0") });
        mc_ear_decorrelate_tilde& obj = an_instance;
        REQUIRE(obj.by_layout());
        REQUIRE(obj.channel_count() == k_channels_050);
        REQUIRE(obj.mc_output_channels(0) == static_cast<long>(k_channels_050));
        REQUIRE(obj.latency() == ear::decorrelatorCompensationDelay());
        start_audio(obj);

        WHEN("an impulse enters the centre channel") {
            const auto response = impulse_response(obj, k_channels_050, k_m000);
            THEN("the centre output is libear's filter for M+000, and the other channels stay silent") {
                const auto expected = ear::designDecorrelator<float>(ear::getLayout("0+5+0"), k_m000);
                REQUIRE(expected.size() == k_taps);
                REQUIRE(largest_difference(response[k_m000], expected) < 1e-6);
                for (size_t ch = 0; ch < k_channels_050; ++ch) {
                    if (ch != k_m000) {
                        REQUIRE(energy(response[ch]) == Approx(0.0).margin(1e-12));
                    }
                }
            }
            THEN("the filter is all-pass: the impulse's energy comes out whole, spread over the taps") {
                REQUIRE(energy(response[k_m000]) == Approx(1.0).epsilon(1e-4));
                REQUIRE(std::abs(response[k_m000][0]) < 0.5);
            }
        }

        WHEN("impulses enter two loudspeakers") {
            const auto centre = impulse_response(obj, k_channels_050, k_m000);
            start_audio(obj);    // fresh filter state
            const auto lfe = impulse_response(obj, k_channels_050, k_lfe);
            THEN("they get different filters") {
                REQUIRE(largest_difference(centre[k_m000], std::vector<float>(lfe[k_lfe].begin(), lfe[k_lfe].end())) > 0.01);
            }
        }

        WHEN("the input carries another channel count") {
            stderr_capture console;
            REQUIRE(obj.mc_input_changed(0, 9) == 0);
            THEN("the count stays the layout's and the mismatch is reported once") {
                REQUIRE(obj.channel_count() == k_channels_050);
                REQUIRE(console.text.str().find("9 channels") != std::string::npos);
            }
        }

        WHEN("chans is set while the layout is in charge") {
            stderr_capture console;
            obj.chans = 10;
            THEN("it stays the layout's count") {
                REQUIRE(obj.channel_count() == k_channels_050);
                REQUIRE(static_cast<int>(obj.chans) == static_cast<int>(k_channels_050));
                REQUIRE(console.text.str().find("layout") != std::string::npos);
            }
        }
    }
}

SCENARIO("mc.ear.decorrelate~ seeds the filters by channel number without a layout") {
    ext_main(nullptr);

    GIVEN("an instance without a layout") {
        test_wrapper_args<mc_ear_decorrelate_tilde> an_instance(atoms{});
        mc_ear_decorrelate_tilde& obj = an_instance;
        REQUIRE(!obj.by_layout());
        REQUIRE(obj.channel_count() == 2);

        WHEN("the input carries six channels") {
            REQUIRE(obj.mc_input_changed(0, 6) == 0);
            THEN("the count follows it") {
                REQUIRE(obj.channel_count() == 6);
                REQUIRE(static_cast<int>(obj.chans) == 6);
                REQUIRE(obj.mc_output_channels(0) == 6);
            }
        }

        WHEN("three channels are decorrelated") {
            obj.chans = 3;
            start_audio(obj);
            const auto first = impulse_response(obj, 3, 0);
            start_audio(obj);
            const auto second = impulse_response(obj, 3, 1);
            THEN("channel 1 gets the filter seeded by 0 and channel 2 the one seeded by 1, which in 0+5+0 are LFE1's and M+000's") {
                // libear seeds a loudspeaker's filter by its rank among the layout's channel names: in
                // 0+5+0 that order is LFE1 M+000 M+030 M+110 M-030 M-110
                const auto layout = ear::getLayout("0+5+0");
                REQUIRE(largest_difference(first[0], ear::designDecorrelator<float>(layout, k_lfe)) < 1e-6);
                REQUIRE(largest_difference(second[1], ear::designDecorrelator<float>(layout, k_m000)) < 1e-6);
                REQUIRE(largest_difference(first[0], obj.filter(0)) < 1e-6);
            }
        }

        WHEN("more channels than can be allocated are connected") {
            stderr_capture console;
            REQUIRE(obj.mc_input_changed(0, 2000) == 0);
            THEN("the count stops at the cap and the excess is reported") {
                REQUIRE(obj.channel_count() == k_max_mc_inputs);
                REQUIRE(console.text.str().find("2000 channels") != std::string::npos);
            }
        }
    }

    GIVEN("an unknown layout") {
        stderr_capture console;
        test_wrapper_args<mc_ear_decorrelate_tilde> an_instance(atoms{ symbol("5+5+5") });
        mc_ear_decorrelate_tilde& obj = an_instance;
        THEN("it is refused and the filters are seeded by channel number") {
            REQUIRE(!obj.by_layout());
            REQUIRE(console.text.str().find("known layouts") != std::string::npos);
        }
    }
}

SCENARIO("mc.ear.decorrelate~ mixes a direct share aligned with the decorrelated signal") {
    ext_main(nullptr);

    GIVEN("a two-channel instance with the audio started") {
        test_wrapper_args<mc_ear_decorrelate_tilde> an_instance(atoms{});
        mc_ear_decorrelate_tilde& obj = an_instance;
        const int delay = ear::decorrelatorCompensationDelay();
        REQUIRE(delay == 255);

        WHEN("diffuse is 0") {
            obj.diffuse = 0.0;
            start_audio(obj);
            const auto response = impulse_response(obj, 2, 0);
            THEN("the impulse comes out delayed by the compensation delay, and nothing else") {
                REQUIRE(response[0][static_cast<size_t>(delay)] == Approx(1.0));
                REQUIRE(energy(response[0]) == Approx(1.0).epsilon(1e-6));
                REQUIRE(obj.latency() == delay);
            }
        }

        WHEN("diffuse is 0 and align is off") {
            obj.diffuse = 0.0;
            obj.align = false;
            start_audio(obj);
            const auto response = impulse_response(obj, 2, 0);
            THEN("the impulse passes with no delay") {
                REQUIRE(response[0][0] == Approx(1.0));
                REQUIRE(energy(response[0]) == Approx(1.0).epsilon(1e-6));
                REQUIRE(obj.latency() == 0);
            }
        }

        WHEN("diffuse is 0.5") {
            obj.diffuse = 0.5;
            start_audio(obj);
            const auto response = impulse_response(obj, 2, 0);
            THEN("both shares are scaled by the square root of a half and sum at the delay") {
                const auto taps = obj.filter(0);
                const double half = std::sqrt(0.5);
                REQUIRE(response[0][static_cast<size_t>(delay)] == Approx(half + half * taps[static_cast<size_t>(delay)]).margin(1e-6));
                REQUIRE(response[0][0] == Approx(half * taps[0]).margin(1e-6));
                REQUIRE(static_cast<double>(obj.diffuse) == Approx(0.5));
            }
        }

        WHEN("diffuse is out of range") {
            obj.diffuse = 7.0;
            THEN("it is clamped") {
                REQUIRE(static_cast<double>(obj.diffuse) == Approx(1.0));
            }
        }

        WHEN("the audio runs with more output channels than allocated") {
            start_audio(obj);
            mc_audio_io io(2, 4, k_block);
            std::fill(io.ins[0].begin(), io.ins[0].end(), 1.0);
            obj(io.input(), io.output());
            THEN("the extra outputs are silent") {
                REQUIRE(energy(io.outs[2]) == Approx(0.0).margin(1e-12));
                REQUIRE(energy(io.outs[3]) == Approx(0.0).margin(1e-12));
            }
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_decorrelate_tilde, "mc.ear.decorrelate~")
