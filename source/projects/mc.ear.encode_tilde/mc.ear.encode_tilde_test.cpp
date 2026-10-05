/// @file
/// @brief   Unit tests for mc.ear.encode~ and the spherical harmonics in ear_max_hoa.h
///          (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cmath>

#include "../shared/ear_max_test.h"
#include "mc.ear.encode_tilde.h"

EARMAX_TEST_FORWARD_ARGUMENTS(mc_ear_encode_tilde)

#include "mc.ear.encode_tilde.cpp"

using namespace earmax_test;

namespace {

constexpr double k_pi = 3.14159265358979323846;
constexpr long k_block = 64;

// ACN order for first order: W(0,0) Y(1,-1) Z(1,0) X(1,1)
constexpr size_t k_w = 0;
constexpr size_t k_y = 1;
constexpr size_t k_z = 2;
constexpr size_t k_x = 3;

struct reference_value {
    int n;
    int m;
    double azimuth;
    double elevation;
    const char* normalization;
    double value;
};

// computed with ear.core.hoa.sph_harm of the Python EAR 2.1.0 (azimuth and
// elevation in degrees, ADM conventions)
const reference_value k_reference[] = {
    { 0, 0, 0.0, 0.0, "SN3D", 1.000000000000 },
    { 1, -1, 30.0, 10.0, "SN3D", 0.492403876506 },
    { 1, 0, 30.0, 10.0, "SN3D", 0.173648177667 },
    { 1, 1, 30.0, 10.0, "SN3D", 0.852868531952 },
    { 2, -2, -120.0, -35.0, "SN3D", 0.503257553747 },
    { 2, 1, 200.0, 60.0, "SN3D", -0.704769465589 },
    { 3, 3, 45.0, -20.0, "SN3D", -0.463855232678 },
    { 3, -2, 10.0, 80.0, "SN3D", 0.019667956428 },
    { 4, 0, 0.0, 45.0, "SN3D", -0.406250000000 },
    { 5, -4, 100.0, -50.0, "SN3D", -0.186490511541 },
    { 0, 0, 0.0, 0.0, "N3D", 1.000000000000 },
    { 1, -1, 30.0, 10.0, "N3D", 0.852868531952 },
    { 1, 0, 30.0, 10.0, "N3D", 0.300767466361 },
    { 1, 1, 30.0, 10.0, "N3D", 1.477211629518 },
    { 2, -2, -120.0, -35.0, "N3D", 1.125318100369 },
    { 2, 1, 200.0, 60.0, "N3D", -1.575912433524 },
    { 3, 3, 45.0, -20.0, "N3D", -1.227245590002 },
    { 3, -2, 10.0, 80.0, "N3D", 0.052036521507 },
    { 4, 0, 0.0, 45.0, "N3D", -1.218750000000 },
    { 5, -4, 100.0, -50.0, "N3D", -0.618519053743 },
    { 0, 0, 0.0, 0.0, "FuMa", 0.707106781187 },
    { 1, -1, 30.0, 10.0, "FuMa", 0.492403876506 },
    { 1, 0, 30.0, 10.0, "FuMa", 0.173648177667 },
    { 1, 1, 30.0, 10.0, "FuMa", 0.852868531952 },
    { 2, -2, -120.0, -35.0, "FuMa", 0.581111768255 },
    { 2, 1, 200.0, 60.0, "FuMa", -0.813797681349 },
    { 3, 3, 45.0, -20.0, "FuMa", -0.586735615940 },
    { 3, -2, 10.0, 80.0, "FuMa", 0.026387332532 },
};

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

std::string normalization_of(mc_ear_encode_tilde& obj)
{
    const symbol value = obj.normalization;
    return value;
}

} // namespace

TEST_CASE("the spherical harmonics match the Python EAR") {
    for (const auto& r : k_reference) {
        hoa::normalization kind = hoa::normalization::SN3D;
        REQUIRE(hoa::normalization_from_name(r.normalization, kind));
        const double value = hoa::sph_harm(r.n, r.m, r.azimuth * k_pi / 180.0, r.elevation * k_pi / 180.0, kind);
        INFO("n=" << r.n << " m=" << r.m << " az=" << r.azimuth << " el=" << r.elevation << " " << r.normalization);
        REQUIRE(value == Approx(r.value).margin(1e-9));
    }
    int n = 0, m = 0;
    hoa::from_acn(8, n, m);
    REQUIRE((n == 2 && m == 2));
    REQUIRE(hoa::to_acn(2, -1) == 5);
    REQUIRE(hoa::component_count(3) == 16);
}

SCENARIO("mc.ear.encode~ encodes objects into an ambisonic scene") {
    ext_main(nullptr);

    GIVEN("a first-order instance") {
        test_wrapper<mc_ear_encode_tilde> an_instance;
        mc_ear_encode_tilde& obj = an_instance;

        REQUIRE(obj.current_order() == 1);
        REQUIRE(obj.component_count() == 4);
        REQUIRE(obj.object_count() == 16);
        REQUIRE(obj.mc_output_channels(0) == 4);
        REQUIRE(normalization_of(obj) == "SN3D");

        THEN("an object at the front encodes to W = X = 1, Y = Z = 0") {
            const auto& g = obj.gains(0);
            REQUIRE(g[k_w] == Approx(1.0));
            REQUIRE(g[k_y] == Approx(0.0).margin(1e-9));
            REQUIRE(g[k_z] == Approx(0.0).margin(1e-9));
            REQUIRE(g[k_x] == Approx(1.0));
        }

        WHEN("objects are positioned with setvalue, applyvalues, a list and a plain message") {
            obj.setvalue(atoms{ 1, symbol("azimuth"), 90.0 });
            obj.applyvalues(atoms{ symbol("elevation"), 0.0, 90.0 });
            obj.setvalue(atoms{ 3, symbol("position"), 30.0, 10.0 });
            THEN("the encoding follows the ADM conventions: left is +Y, up is +Z") {
                REQUIRE(obj.gains(0)[k_y] == Approx(1.0));
                REQUIRE(obj.gains(0)[k_x] == Approx(0.0).margin(1e-9));
                REQUIRE(obj.gains(1)[k_z] == Approx(1.0));
                REQUIRE(obj.gains(1)[k_x] == Approx(0.0).margin(1e-9));
                REQUIRE(obj.gains(2)[k_y] == Approx(0.492403876506));
                REQUIRE(obj.gains(2)[k_z] == Approx(0.173648177667));
                REQUIRE(obj.gains(2)[k_x] == Approx(0.852868531952));
            }
            obj.list(atoms{ -90.0, 0.0 });
            THEN("a list positions every object") {
                for (size_t i = 0; i < obj.object_count(); ++i) {
                    REQUIRE(obj.gains(i)[k_y] == Approx(-1.0));
                }
            }
            obj.anything(atoms{ symbol("gain"), 0.5 });
            THEN("the gain scales every component") {
                REQUIRE(obj.gains(0)[k_w] == Approx(0.5));
                REQUIRE(obj.gains(0)[k_y] == Approx(-0.5));
            }
        }

        WHEN("an object uses cartesian coordinates") {
            obj.setvalue(atoms{ 1, symbol("cartesian"), 1 });
            obj.setvalue(atoms{ 1, symbol("position"), 1.0, 0.0, 0.0 });    // x = 1: right
            THEN("the direction comes from the ADM conversion to polar (BS.2127 section 10), not a plain atan2") {
                const ear::PolarPosition polar = ear::conversion::pointCartToPolar(ear::CartesianPosition(1.0, 0.0, 0.0));
                REQUIRE(polar.azimuth < -45.0);    // to the right, but not at -90: the ADM mapping warps the cube
                REQUIRE(polar.elevation == Approx(0.0).margin(1e-9));
                const double az = polar.azimuth * k_pi / 180.0;
                REQUIRE(obj.gains(0)[k_y] == Approx(std::sin(az)));
                REQUIRE(obj.gains(0)[k_x] == Approx(std::cos(az)));
                REQUIRE(obj.gains(0)[k_w] == Approx(1.0));
            }
        }

        WHEN("parameters that do not affect the encoding are sent") {
            obj.setvalue(atoms{ 1, symbol("diffuse"), 0.7 });
            obj.anything(atoms{ symbol("width"), 30.0 });
            THEN("they are accepted and the encoding is unchanged") {
                REQUIRE(obj.metadata(0).otm.diffuse == Approx(0.7));
                REQUIRE(obj.gains(0)[k_x] == Approx(1.0));
            }
        }

        WHEN("the normalization is changed") {
            obj.normalization = symbol("N3D");
            THEN("first-order components scale by sqrt(3)") {
                REQUIRE(obj.gains(0)[k_x] == Approx(std::sqrt(3.0)));
                REQUIRE(obj.gains(0)[k_w] == Approx(1.0));
            }
            obj.normalization = symbol("FuMa");
            THEN("FuMa scales W by 1/sqrt(2)") {
                REQUIRE(obj.gains(0)[k_w] == Approx(1.0 / std::sqrt(2.0)));
            }
            obj.normalization = symbol("ambix");
            THEN("an unknown name is rejected") {
                REQUIRE(normalization_of(obj) == "FuMa");
            }
            obj.order = 5;
            THEN("FuMa limits the order to 3") {
                REQUIRE(obj.current_order() == 3);
                REQUIRE(obj.component_count() == 16);
            }
        }

        WHEN("the order is raised") {
            obj.order = 3;
            THEN("there are 16 components with the right values") {
                REQUIRE(obj.component_count() == 16);
                REQUIRE(obj.mc_output_channels(0) == 16);
                obj.setvalue(atoms{ 1, symbol("position"), 45.0, -20.0 });
                REQUIRE(obj.gains(0)[hoa::to_acn(3, 3)] == Approx(-0.463855232678));
            }
            obj.normalization = symbol("FuMa");
            THEN("FuMa is defined up to order 3, so it is accepted here") {
                REQUIRE(normalization_of(obj) == "FuMa");
            }
            obj.normalization = symbol("SN3D");
            obj.order = 4;
            obj.normalization = symbol("FuMa");
            THEN("FuMa cannot be selected above order 3") {
                REQUIRE(normalization_of(obj) == "SN3D");
                REQUIRE(obj.current_order() == 4);
            }
        }

        obj.vector_size(k_block);
        obj.samplerate(48000.0);
        obj.ramp = 0.0;
        obj.dspsetup(atoms{ 48000.0, k_block });

        WHEN("two objects are encoded") {
            obj.setvalue(atoms{ 1, symbol("azimuth"), 30.0 });
            obj.setvalue(atoms{ 1, symbol("elevation"), 10.0 });
            obj.setvalue(atoms{ 2, symbol("azimuth"), 0.0 });
            mc_audio_io io(2, 4, k_block);
            std::fill(io.ins[0].begin(), io.ins[0].end(), 1.0);
            std::fill(io.ins[1].begin(), io.ins[1].end(), 1.0);
            obj(io.input(), io.output());
            THEN("the output is the sum of the encoded objects, without latency") {
                REQUIRE(io.outs[k_w][k_block - 1] == Approx(2.0));
                REQUIRE(io.outs[k_y][k_block - 1] == Approx(0.492403876506));
                REQUIRE(io.outs[k_z][k_block - 1] == Approx(0.173648177667));
                REQUIRE(io.outs[k_x][k_block - 1] == Approx(1.0 + 0.852868531952));
                REQUIRE(io.outs[k_w][0] == Approx(2.0));
            }
        }

        WHEN("an object is given its own ramp time") {
            obj.setvalue(atoms{ 1, symbol("ramp"), 100.0 });    // 4800 samples at 48 kHz
            obj.setvalue(atoms{ 1, symbol("azimuth"), 90.0 });    // Y goes from 0 to 1
            mc_audio_io io(1, 4, k_block);
            std::fill(io.ins[0].begin(), io.ins[0].end(), 1.0);
            obj(io.input(), io.output());
            THEN("the encoding gains interpolate over that time instead of the ramp attribute") {
                REQUIRE(io.outs[k_y][k_block - 1] == Approx(64.0 / 4800.0).margin(2.0 / 4800.0));
                REQUIRE(io.outs[k_w][k_block - 1] == Approx(1.0));
            }
            AND_WHEN("a negative ramp returns the object to the attribute") {
                obj.setvalue(atoms{ 1, symbol("ramp"), -1.0 });
                obj.setvalue(atoms{ 1, symbol("azimuth"), -90.0 });
                obj(io.input(), io.output());
                THEN("the change is immediate again") {
                    REQUIRE(io.outs[k_y][k_block - 1] == Approx(-1.0));
                }
            }
        }

        WHEN("more input channels than objects are connected") {
            REQUIRE(obj.mc_input_changed(0, 20) == 0);
            mc_audio_io io(20, 4, k_block);
            for (auto& in : io.ins) std::fill(in.begin(), in.end(), 1.0);
            obj(io.input(), io.output());
            THEN("only the allocated objects are encoded") {
                REQUIRE(io.outs[k_w][k_block - 1] == Approx(16.0));
            }
        }
    }

    GIVEN("an instance created with the order as argument") {
        test_wrapper_args<mc_ear_encode_tilde> an_instance(atoms{ 2 });
        mc_ear_encode_tilde& obj = an_instance;
        THEN("the order and the component count follow") {
            REQUIRE(obj.current_order() == 2);
            REQUIRE(obj.component_count() == 9);
            REQUIRE(obj.gains(0).size() == 9);
        }
        obj.order = 20;
        THEN("an order above 8 is clamped") {
            REQUIRE(obj.current_order() == hoa::k_max_order);
        }
    }
}


SCENARIO("mc.ear.encode~ grows its allocation when a message or the input addresses more objects") {
    ext_main(nullptr);

    GIVEN("an instance with the default 16 objects") {
        test_wrapper_args<mc_ear_encode_tilde> an_instance(atoms{ 1 });
        mc_ear_encode_tilde& obj = an_instance;
        REQUIRE(obj.object_count() == 16);

        WHEN("setvalue addresses object 40") {
            obj.setvalue(atoms{ 40, symbol("azimuth"), 90.0 });
            THEN("40 objects are allocated and chans follows") {
                REQUIRE(obj.object_count() == 40);
                REQUIRE(static_cast<int>(obj.chans) == 40);
            }
        }

        WHEN("the input signal carries 20 channels") {
            REQUIRE(obj.mc_input_changed(0, 20) == 0);
            THEN("20 objects are allocated") {
                REQUIRE(obj.object_count() == 20);
            }
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_encode_tilde, "mc.ear.encode~")
