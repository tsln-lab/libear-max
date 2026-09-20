/// @file
/// @brief   Unit tests for ear.direct (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min_unittest.h"
#include "ear.direct.cpp"

namespace {

// 0+5+0 channel order in libear: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr int k_m030 = 0;
constexpr int k_m000 = 2;
constexpr int k_lfe = 3;
constexpr int k_channels_050 = 6;

std::vector<double> last_list(void* obj, int outlet_index)
{
    auto& output = *c74::max::object_getoutput(obj, outlet_index);
    REQUIRE(!output.empty());
    const auto& msg = output.back();
    std::vector<double> values;
    for (const auto& a : msg) {
        if (a.a_type == c74::max::A_SYM) {
            continue;
        }
        values.push_back(static_cast<double>(a));
    }
    return values;
}

} // namespace

SCENARIO("ear.direct maps DirectSpeakers channels onto a layout") {
    ext_main(nullptr);

    GIVEN("an instance with the default 0+5+0 layout") {
        test_wrapper<ear_direct> an_instance;
        ear_direct& obj = an_instance;

        REQUIRE(obj.layout == symbol("0+5+0"));
        REQUIRE(obj.channel_count() == k_channels_050);

        WHEN("a speaker label matching a loudspeaker is set") {
            obj.speakerlabel({ symbol("M+030") });
            THEN("that loudspeaker gets unity gain") {
                auto gains = last_list(obj, 0);
                REQUIRE(gains.size() == k_channels_050);
                REQUIRE(gains[k_m030] == Approx(1.0));
                REQUIRE(gains[k_m000] == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("only a position is given") {
            obj.speakerlabel({});
            obj.list({ 0.0, 0.0 });
            THEN("the channel is panned to the front loudspeaker") {
                auto gains = last_list(obj, 0);
                REQUIRE(gains[k_m000] == Approx(1.0));
            }
        }

        WHEN("the channel is marked as LFE") {
            obj.speakerlabel({ symbol("LFE1") });
            obj.lfe = true;
            THEN("only the LFE loudspeaker receives signal") {
                auto gains = last_list(obj, 0);
                REQUIRE(gains[k_lfe] == Approx(1.0));
                REQUIRE(gains[k_m000] == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("the layout is switched to 0+2+0") {
            obj.layout = "0+2+0";
            obj.speakerlabel({ symbol("M+030") });
            THEN("two gains are produced") {
                REQUIRE(obj.channel_count() == 2);
                auto gains = last_list(obj, 0);
                REQUIRE(gains.size() == 2);
                REQUIRE(gains[0] == Approx(1.0));
            }
        }
    }
}
