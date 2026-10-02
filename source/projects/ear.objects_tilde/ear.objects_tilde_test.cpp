/// @file
/// @brief   Unit tests for ear.objects~ (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include "../shared/ear_max_test.h"
#include "ear.objects_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(ear_objects_tilde)

#include "ear.objects_tilde.cpp"

using namespace earmax_test;

namespace {

// 0+5+0 channel order in libear: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr int k_m030 = 0;
constexpr int k_m000 = 2;
constexpr size_t k_channels_050 = 6;
constexpr long k_block = 64;

/// Simple owner of one input vector and N output vectors for driving the vector operator.
struct audio_io {
    std::vector<double> in;
    std::vector<std::vector<double>> outs;
    std::vector<double*> in_ptrs;
    std::vector<double*> out_ptrs;

    audio_io(size_t channels, long frames)
        : in(frames, 0.0), outs(channels, std::vector<double>(frames, 0.0))
    {
        in_ptrs.push_back(in.data());
        for (auto& o : outs) {
            out_ptrs.push_back(o.data());
        }
    }

    audio_bundle input()
    {
        return audio_bundle{ in_ptrs.data(), 1, static_cast<long>(in.size()) };
    }

    audio_bundle output()
    {
        return audio_bundle{ out_ptrs.data(), static_cast<long>(outs.size()), static_cast<long>(in.size()) };
    }
};

double sum_abs(const std::vector<double>& v)
{
    double s = 0.0;
    for (auto x : v) {
        s += std::abs(x);
    }
    return s;
}

} // namespace

SCENARIO("ear.objects~ renders a mono object to loudspeaker signals") {
    ext_main(nullptr);

    GIVEN("an instance created with the 0+5+0 layout") {
        test_wrapper_args<ear_objects_tilde> an_instance(atoms{ symbol("0+5+0") });
        ear_objects_tilde& obj = an_instance;

        REQUIRE(obj.channel_count() == k_channels_050);
        REQUIRE(obj.outlets().size() == k_channels_050);

        obj.vector_size(k_block);
        obj.samplerate(48000.0);
        obj.dspsetup(atoms{ 48000.0, k_block });

        WHEN("decorrelation is off and a DC signal is rendered at the default (front) position") {
            obj.decorrelate = false;
            audio_io io(k_channels_050, k_block);
            std::fill(io.in.begin(), io.in.end(), 1.0);
            obj(io.input(), io.output());

            THEN("the signal appears on M+000 only, with no latency") {
                REQUIRE(io.outs[k_m000][0] == Approx(1.0));
                REQUIRE(io.outs[k_m000][k_block - 1] == Approx(1.0));
                REQUIRE(sum_abs(io.outs[k_m030]) == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("the layout attribute is changed after creation") {
            obj.layout = "4+5+0";
            THEN("the change is refused because the outlet count is fixed") {
                REQUIRE(obj.layout == symbol("0+5+0"));
                REQUIRE(obj.channel_count() == k_channels_050);
            }
        }

        WHEN("the position moves to M+030 with a zero ramp") {
            obj.decorrelate = false;
            obj.ramp = 0.0;
            obj.list(atoms{ 30.0, 0.0 });
            audio_io io(k_channels_050, k_block);
            std::fill(io.in.begin(), io.in.end(), 1.0);
            obj(io.input(), io.output());

            THEN("the new gains are in effect from the first sample") {
                REQUIRE(io.outs[k_m030][0] == Approx(1.0));
                REQUIRE(io.outs[k_m000][0] == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("the position moves with a ramp longer than one vector") {
            obj.decorrelate = false;
            obj.ramp = 10.0;    // 480 samples at 48 kHz
            obj.list(atoms{ 0.0, 0.0 });
            audio_io warm(k_channels_050, k_block);
            obj(warm.input(), warm.output());    // settle the ramp to the front position
            for (int i = 0; i < 10; ++i) {
                obj(warm.input(), warm.output());
            }

            obj.list(atoms{ 30.0, 0.0 });
            audio_io io(k_channels_050, k_block);
            std::fill(io.in.begin(), io.in.end(), 1.0);
            obj(io.input(), io.output());

            THEN("the gain on M+030 rises gradually and M+000 falls") {
                REQUIRE(io.outs[k_m030][0] > 0.0);
                REQUIRE(io.outs[k_m030][0] < 0.1);
                REQUIRE(io.outs[k_m030][k_block - 1] > io.outs[k_m030][0]);
                REQUIRE(io.outs[k_m030][k_block - 1] < 1.0);
                REQUIRE(io.outs[k_m000][k_block - 1] < io.outs[k_m000][0]);
            }
        }

        WHEN("decorrelation is on and an impulse is rendered fully direct") {
            obj.decorrelate = true;
            obj.ramp = 0.0;
            obj.diffuse = 0.0;
            obj.list(atoms{ 0.0, 0.0 });

            const int delay = obj.latency();
            REQUIRE(delay == ear::decorrelatorCompensationDelay());

            audio_io io(k_channels_050, k_block);
            std::vector<double> front;    // M+000 output, concatenated over vectors
            for (long block = 0; block * k_block <= delay + k_block; ++block) {
                std::fill(io.in.begin(), io.in.end(), 0.0);
                if (block == 0) {
                    io.in[0] = 1.0;
                }
                obj(io.input(), io.output());
                front.insert(front.end(), io.outs[k_m000].begin(), io.outs[k_m000].end());
            }

            THEN("the impulse comes out on M+000 delayed by the decorrelator compensation delay") {
                REQUIRE(front[delay] == Approx(1.0));
                REQUIRE(sum_abs(front) == Approx(1.0).margin(1e-6));
            }
        }

        WHEN("decorrelation is on and the object is fully diffuse") {
            obj.decorrelate = true;
            obj.ramp = 0.0;
            obj.diffuse = 1.0;
            obj.list(atoms{ 0.0, 0.0 });

            audio_io io(k_channels_050, k_block);
            double energy_front = 0.0;
            for (long block = 0; block < 16; ++block) {
                std::fill(io.in.begin(), io.in.end(), 0.0);
                if (block == 0) {
                    io.in[0] = 1.0;
                }
                obj(io.input(), io.output());
                for (auto v : io.outs[k_m000]) {
                    energy_front += v * v;
                }
            }

            THEN("the decorrelation filter spreads the impulse over time while keeping its energy") {
                REQUIRE(energy_front == Approx(1.0).epsilon(0.05));
            }
        }
    }

    GIVEN("an instance created with the 4+5+0 layout") {
        test_wrapper_args<ear_objects_tilde> an_instance(atoms{ symbol("4+5+0") });
        ear_objects_tilde& obj = an_instance;

        THEN("there is one signal outlet per loudspeaker") {
            REQUIRE(obj.layout == symbol("4+5+0"));
            REQUIRE(obj.channel_count() == 10);
            REQUIRE(obj.outlets().size() == 10);
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(ear_objects_tilde, "ear.objects~")
