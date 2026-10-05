/// @file
/// @brief   Unit tests for mc.ear.hoa~ (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cmath>

#include "../shared/ear_max_test.h"
#include "mc.ear.hoa_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(mc_ear_hoa_tilde)

#include "mc.ear.hoa_tilde.cpp"

using namespace earmax_test;

namespace {

// 0+5+0 channel order: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr size_t k_channels_050 = 6;
constexpr size_t k_m030 = 0;
constexpr size_t k_m000 = 2;
constexpr size_t k_lfe = 3;
constexpr size_t k_m110 = 4;
constexpr long k_block = 64;

// ACN order for first order: W(0,0) Y(1,-1) Z(1,0) X(1,1)
constexpr size_t k_w = 0;
constexpr size_t k_y = 1;
constexpr size_t k_z = 2;
constexpr size_t k_x = 3;

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

constexpr double k_pi = 3.14159265358979323846;

/// the normalization attribute as text
std::string normalization_of(mc_ear_hoa_tilde& obj)
{
    const symbol value = obj.normalization;
    return value;
}

/// gain of a plane wave from (azimuth, elevation) at loudspeaker `speaker`
/// for a first-order SN3D scene decoded with `obj`
double plane_wave_gain(const mc_ear_hoa_tilde& obj, double azimuth_deg, double elevation_deg, size_t speaker)
{
    const double az = azimuth_deg * k_pi / 180.0;
    const double el = elevation_deg * k_pi / 180.0;
    // SN3D first-order encoding in ACN order (ADM azimuth is anticlockwise)
    const double components[4] = { 1.0, std::sin(az) * std::cos(el), std::sin(el), std::cos(az) * std::cos(el) };
    double gain = 0.0;
    for (size_t i = 0; i < 4; ++i) {
        gain += components[i] * obj.gains(i)[speaker];
    }
    return gain;
}

} // namespace

SCENARIO("mc.ear.hoa~ decodes an ambisonic scene to a multichannel loudspeaker signal") {
    ext_main(nullptr);

    GIVEN("a first-order instance decoding to 0+5+0") {
        test_wrapper_args<mc_ear_hoa_tilde> an_instance(atoms{ symbol("0+5+0") });
        mc_ear_hoa_tilde& obj = an_instance;

        REQUIRE(obj.channel_count() == k_channels_050);
        REQUIRE(obj.input_count() == 4);
        REQUIRE(obj.mc_output_channels(0) == static_cast<long>(k_channels_050));
        REQUIRE(normalization_of(obj) == "SN3D");

        THEN("the decoding matrix matches libear's HOA gain calculator") {
            ear::GainCalculatorHOA calc(ear::getLayout("0+5+0"));
            ear::HOATypeMetadata meta;
            meta.orders = { 0, 1, 1, 1 };
            meta.degrees = { 0, -1, 0, 1 };
            std::vector<std::vector<float>> expected(4, std::vector<float>(k_channels_050, 0.0f));
            calc.calculate(meta, expected);
            for (size_t i = 0; i < 4; ++i) {
                for (size_t ch = 0; ch < k_channels_050; ++ch) {
                    REQUIRE(obj.gains(i)[ch] == Approx(expected[i][ch]));
                }
            }
        }

        THEN("plane waves land on the loudspeakers facing them and never reach the LFE") {
            // first order is broad: a front wave spreads over L, C and R, symmetrically
            REQUIRE(plane_wave_gain(obj, 0.0, 0.0, k_m000) > 0.3);
            REQUIRE(plane_wave_gain(obj, 0.0, 0.0, k_m030) == Approx(plane_wave_gain(obj, 0.0, 0.0, k_m030 + 1)).epsilon(1e-3));
            REQUIRE(plane_wave_gain(obj, 0.0, 0.0, k_m000) > plane_wave_gain(obj, 0.0, 0.0, k_m110));
            REQUIRE(plane_wave_gain(obj, 0.0, 0.0, k_m030) > plane_wave_gain(obj, 0.0, 0.0, k_m110));
            REQUIRE(plane_wave_gain(obj, 30.0, 0.0, k_m030) > plane_wave_gain(obj, 30.0, 0.0, k_m030 + 1));
            REQUIRE(plane_wave_gain(obj, 30.0, 0.0, k_m030) > plane_wave_gain(obj, 30.0, 0.0, k_m110));
            REQUIRE(plane_wave_gain(obj, 110.0, 0.0, k_m110) > plane_wave_gain(obj, 110.0, 0.0, k_m110 + 1));
            REQUIRE(plane_wave_gain(obj, 110.0, 0.0, k_m110) > plane_wave_gain(obj, 110.0, 0.0, k_m030));
            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(obj.gains(i)[k_lfe] == Approx(0.0).margin(1e-9));
            }
        }

        THEN("the ACN index maps to order and degree") {
            int n = 0, m = 0;
            mc_ear_hoa_tilde::acn_to_order_degree(0, n, m);
            REQUIRE((n == 0 && m == 0));
            mc_ear_hoa_tilde::acn_to_order_degree(1, n, m);
            REQUIRE((n == 1 && m == -1));
            mc_ear_hoa_tilde::acn_to_order_degree(3, n, m);
            REQUIRE((n == 1 && m == 1));
            mc_ear_hoa_tilde::acn_to_order_degree(8, n, m);
            REQUIRE((n == 2 && m == 2));
        }

        WHEN("the order is raised to 3") {
            obj.order = 3;
            THEN("there are 16 components with gains for every loudspeaker") {
                REQUIRE(obj.input_count() == 16);
                REQUIRE(obj.gains(15).size() == k_channels_050);
            }
        }

        WHEN("an invalid order or normalization is set") {
            obj.order = 20;
            obj.normalization = symbol("ambix");
            THEN("the order is clamped and the normalization kept") {
                REQUIRE(static_cast<int>(obj.order) == mc_ear_hoa_tilde::k_max_order);
                REQUIRE(normalization_of(obj) == "SN3D");
            }
        }

        WHEN("the normalization is N3D") {
            const double sn3d_x = obj.gains(k_x)[k_m000];
            obj.normalization = symbol("N3D");
            THEN("first-order components are scaled by 1/sqrt(3) relative to SN3D, the zeroth is unchanged") {
                REQUIRE(obj.gains(k_x)[k_m000] == Approx(sn3d_x / std::sqrt(3.0)));
            }
        }

        obj.vector_size(k_block);
        obj.samplerate(48000.0);
        obj.ramp = 0.0;
        obj.dspsetup(atoms{ 48000.0, k_block });

        THEN("the output is delayed by the decorrelator compensation delay by default (align)") {
            REQUIRE(static_cast<bool>(obj.align));
            REQUIRE(obj.latency() == ear::decorrelatorCompensationDelay());
        }

        WHEN("a front plane wave impulse is decoded with align on") {
            mc_audio_io io(4, k_channels_050, k_block);
            std::vector<double> centre;
            for (long block = 0; block < 8; ++block) {
                for (auto& in : io.ins) std::fill(in.begin(), in.end(), 0.0);
                if (block == 0) {
                    io.ins[k_w][0] = 1.0;
                    io.ins[k_x][0] = 1.0;
                }
                obj(io.input(), io.output());
                centre.insert(centre.end(), io.outs[k_m000].begin(), io.outs[k_m000].end());
            }
            THEN("it arrives on M+000 one latency later with the decoded gain") {
                const auto latency = static_cast<size_t>(obj.latency());
                for (size_t n = 0; n < latency; ++n) {
                    REQUIRE(centre[n] == Approx(0.0).margin(1e-12));
                }
                REQUIRE(centre[latency] == Approx(plane_wave_gain(obj, 0.0, 0.0, k_m000)));
            }
        }

        WHEN("align is off and a constant scene is decoded") {
            obj.align = false;
            REQUIRE(obj.latency() == 0);
            mc_audio_io io(4, k_channels_050, k_block);
            std::fill(io.ins[k_w].begin(), io.ins[k_w].end(), 1.0);
            std::fill(io.ins[k_x].begin(), io.ins[k_x].end(), 1.0);
            obj(io.input(), io.output());
            THEN("the output follows the decoding matrix within the first block") {
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(plane_wave_gain(obj, 0.0, 0.0, k_m000)));
                REQUIRE(io.outs[k_lfe][k_block - 1] == Approx(0.0).margin(1e-12));
            }
        }

        WHEN("the order is lowered while the audio runs") {
            obj.align = false;
            obj.order = 2;
            obj.dspsetup(atoms{ 48000.0, k_block });    // nine components allocated
            mc_audio_io io(9, k_channels_050, k_block);
            for (auto& in : io.ins) std::fill(in.begin(), in.end(), 1.0);
            obj(io.input(), io.output());
            const double second_order = io.outs[k_m000][k_block - 1];
            obj.order = 1;
            obj(io.input(), io.output());
            THEN("the dropped components are silenced instead of keeping their old gains") {
                double expected = 0.0;
                for (size_t i = 0; i < 4; ++i) {
                    expected += obj.gains(i)[k_m000];
                }
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(expected));
                REQUIRE(io.outs[k_m000][k_block - 1] != Approx(second_order));
            }
        }

        WHEN("a single channel is connected, as mc.ear.play~ carries for a file without a scene") {
            REQUIRE(obj.mc_input_changed(0, 1) == 0);
            obj.align = false;
            mc_audio_io io(1, k_channels_050, k_block);
            std::fill(io.ins[0].begin(), io.ins[0].end(), 1.0);
            obj(io.input(), io.output());
            THEN("it is decoded as W alone, without a channel count warning") {
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(obj.gains(0)[k_m000]));
            }
        }

        WHEN("more input channels than components are connected") {
            REQUIRE(obj.mc_input_changed(0, 9) == 0);
            obj.align = false;
            mc_audio_io io(9, k_channels_050, k_block);
            for (auto& in : io.ins) std::fill(in.begin(), in.end(), 1.0);
            obj(io.input(), io.output());
            THEN("only the first-order components are decoded") {
                double expected = 0.0;
                for (size_t i = 0; i < 4; ++i) {
                    expected += obj.gains(i)[k_m000];
                }
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(expected));
            }
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_hoa_tilde, "mc.ear.hoa~")
