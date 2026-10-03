/// @file
/// @brief   Unit tests for ear.hoa (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cmath>

#include "../shared/ear_max_test.h"
#include "ear.hoa.h"

EARMAX_TEST_FORWARD_ARGUMENTS(ear_hoa)

#include "ear.hoa.cpp"

using namespace earmax_test;

namespace {

// 0+5+0 channel order: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr size_t k_channels_050 = 6;
constexpr size_t k_lfe = 3;

enum outlet_index { k_rows = 0, k_matrix = 1, k_info = 2 };

/// every message sent from an outlet, as its numeric atoms (selectors skipped)
std::vector<std::vector<double>> lists(void* obj, int outlet_index)
{
    std::vector<std::vector<double>> out;
    for (const auto& msg : *c74::max::object_getoutput(obj, outlet_index)) {
        std::vector<double> values;
        for (const auto& a : msg) {
            if (a.a_type != c74::max::A_SYM) {
                values.push_back(static_cast<double>(a));
            }
        }
        out.push_back(values);
    }
    return out;
}

/// the selector of the most recent message sent from an outlet
std::string last_selector(void* obj, int outlet_index)
{
    auto& output = *c74::max::object_getoutput(obj, outlet_index);
    REQUIRE(!output.empty());
    const auto& msg = output.back();
    REQUIRE(!msg.empty());
    REQUIRE(msg.front().a_type == c74::max::A_SYM);
    return msg.front().a_w.w_sym->s_name;
}

void clear_outputs(void* obj)
{
    for (int i = 0; i < 3; ++i) {
        c74::max::object_getoutput(obj, i)->clear();
    }
}

/// libear's decoding matrix for a layout, order and normalization
std::vector<std::vector<float>> reference_matrix(const std::string& layout, int order, const std::string& normalization)
{
    ear::GainCalculatorHOA calc(ear::getLayout(layout));
    ear::HOATypeMetadata meta;
    meta.normalization = normalization;
    const size_t count = static_cast<size_t>((order + 1) * (order + 1));
    for (size_t i = 0; i < count; ++i) {
        int n = 0, m = 0;
        hoa::from_acn(static_cast<int>(i), n, m);
        meta.orders.push_back(n);
        meta.degrees.push_back(m);
    }
    std::vector<std::vector<float>> matrix(count, std::vector<float>(ear::getLayout(layout).channels().size(), 0.0f));
    calc.calculate(meta, matrix);
    return matrix;
}

} // namespace

SCENARIO("ear.hoa outputs the decoding matrix of an ambisonic scene") {
    ext_main(nullptr);

    GIVEN("an instance with the default 0+5+0 layout and first order") {
        test_wrapper<ear_hoa> an_instance;
        ear_hoa& obj = an_instance;

        REQUIRE(obj.layout == symbol("0+5+0"));
        REQUIRE(obj.order == 1);
        REQUIRE(obj.channel_count() == k_channels_050);
        REQUIRE(obj.input_count() == 4);

        WHEN("a bang is sent") {
            clear_outputs(obj);
            obj.bang();
            const auto rows = lists(obj, k_rows);
            const auto entries = lists(obj, k_matrix);
            const auto expected = reference_matrix("0+5+0", 1, "SN3D");

            THEN("one list per component carries its channel number and a gain per loudspeaker, as libear computes them") {
                REQUIRE(rows.size() == 4);
                for (size_t i = 0; i < 4; ++i) {
                    REQUIRE(rows[i].size() == 1 + k_channels_050);
                    REQUIRE(rows[i][0] == Approx(static_cast<double>(i + 1)));
                    for (size_t ch = 0; ch < k_channels_050; ++ch) {
                        REQUIRE(rows[i][1 + ch] == Approx(expected[i][ch]));
                        REQUIRE(obj.gains(i)[ch] == Approx(expected[i][ch]));
                    }
                    REQUIRE(rows[i][1 + k_lfe] == Approx(0.0).margin(1e-9));    // the LFE stays silent
                }
            }

            THEN("the matrix~ messages clear the matrix, then cover every entry, inputs and outputs 0-based") {
                REQUIRE(entries.size() == 1 + 4 * k_channels_050);
                REQUIRE(entries.front().empty());    // 'clear' carries no numbers
                REQUIRE(c74::max::object_getoutput(obj, k_matrix)->front().front().a_type == c74::max::A_SYM);
                for (size_t i = 0; i < 4; ++i) {
                    for (size_t ch = 0; ch < k_channels_050; ++ch) {
                        const auto& e = entries[1 + i * k_channels_050 + ch];
                        REQUIRE(e.size() == 3);
                        REQUIRE(e[0] == Approx(static_cast<double>(i)));
                        REQUIRE(e[1] == Approx(static_cast<double>(ch)));
                        REQUIRE(e[2] == Approx(expected[i][ch]));
                    }
                }
            }
        }

        WHEN("the order is raised to 3") {
            clear_outputs(obj);
            obj.order = 3;
            THEN("the 16 components are output at once, as the attribute changed") {
                REQUIRE(obj.input_count() == 16);
                const auto rows = lists(obj, k_rows);
                REQUIRE(rows.size() == 16);
                REQUIRE(rows[15][0] == Approx(16.0));
                REQUIRE(lists(obj, k_matrix).size() == 1 + 16 * k_channels_050);
                const auto expected = reference_matrix("0+5+0", 3, "SN3D");
                for (size_t ch = 0; ch < k_channels_050; ++ch) {
                    REQUIRE(rows[15][1 + ch] == Approx(expected[15][ch]));
                }
            }
        }

        WHEN("the normalization is N3D") {
            obj.normalization = "N3D";
            THEN("the first-order gains are scaled by 1/sqrt(3) against SN3D, the zeroth order unchanged") {
                const auto sn3d = reference_matrix("0+5+0", 1, "SN3D");
                for (size_t ch = 0; ch < k_channels_050; ++ch) {
                    REQUIRE(obj.gains(0)[ch] == Approx(sn3d[0][ch]));
                    REQUIRE(obj.gains(3)[ch] == Approx(sn3d[3][ch] / std::sqrt(3.0)).margin(1e-6));
                }
            }
        }

        WHEN("an invalid normalization or order is given") {
            obj.normalization = "AmbiX";
            obj.order = 12;
            THEN("the normalization is kept and the order clamped") {
                const symbol norm = obj.normalization;
                REQUIRE(norm == symbol("SN3D"));
                REQUIRE(obj.order == 8);
                REQUIRE(obj.input_count() == 81);
            }
        }

        WHEN("the layout is switched to 0+2+0") {
            clear_outputs(obj);
            obj.layout = "0+2+0";
            THEN("each component has two gains") {
                REQUIRE(obj.channel_count() == 2);
                const auto rows = lists(obj, k_rows);
                REQUIRE(rows.size() == 4);
                REQUIRE(rows[0].size() == 3);
                const auto expected = reference_matrix("0+2+0", 1, "SN3D");
                REQUIRE(rows[0][1] == Approx(expected[0][0]));
                REQUIRE(rows[0][2] == Approx(expected[0][1]));
            }
        }

        WHEN("autocalc is off") {
            obj.autocalc = false;
            clear_outputs(obj);
            obj.order = 2;
            THEN("a change outputs nothing until a bang") {
                REQUIRE(c74::max::object_getoutput(obj, k_rows)->empty());
                obj.bang();
                REQUIRE(lists(obj, k_rows).size() == 9);
            }
        }

        WHEN("the components are asked for") {
            clear_outputs(obj);
            obj.components();
            THEN("the info outlet gives the order and degree of each ACN channel") {
                REQUIRE(last_selector(obj, k_info) == "components");
                REQUIRE(lists(obj, k_info).back() == std::vector<double>{ 0, 0, 1, -1, 1, 0, 1, 1 });
            }
        }

        WHEN("the channels are asked for") {
            clear_outputs(obj);
            obj.channels();
            THEN("the info outlet names the loudspeakers") {
                REQUIRE(last_selector(obj, k_info) == "channels");
            }
        }
    }

    GIVEN("an instance created with a layout argument, at second order in FuMa") {
        test_wrapper_args<ear_hoa> an_instance(atoms{ symbol("4+5+0") });
        ear_hoa& obj = an_instance;
        obj.order = 2;
        obj.normalization = "FuMa";

        THEN("the layout argument is honoured and the matrix matches libear's") {
            REQUIRE(obj.layout == symbol("4+5+0"));
            REQUIRE(obj.channel_count() == 10);
            REQUIRE(obj.order == 2);
            REQUIRE(obj.input_count() == 9);
            const auto expected = reference_matrix("4+5+0", 2, "FuMa");
            for (size_t i = 0; i < 9; ++i) {
                for (size_t ch = 0; ch < 10; ++ch) {
                    REQUIRE(obj.gains(i)[ch] == Approx(expected[i][ch]));
                }
            }
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(ear_hoa, "ear.hoa")
