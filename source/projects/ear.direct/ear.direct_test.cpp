/// @file
/// @brief   Unit tests for ear.direct (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include "../shared/ear_max_test.h"
#include "ear.direct.h"

EARMAX_TEST_FORWARD_ARGUMENTS(ear_direct)

#include "ear.direct.cpp"

using namespace earmax_test;

namespace {

// 0+5+0 channel order in libear: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr int k_m030 = 0;
constexpr int k_m000 = 2;
constexpr int k_lfe = 3;
constexpr int k_channels_050 = 6;

} // namespace

SCENARIO("ear.direct maps DirectSpeakers channels onto a layout") {
    ext_main(nullptr);

    GIVEN("an instance with the default 0+5+0 layout") {
        test_wrapper<ear_direct> an_instance;
        ear_direct& obj = an_instance;

        REQUIRE(obj.layout == symbol("0+5+0"));
        REQUIRE(obj.channel_count() == k_channels_050);

        WHEN("a speaker label matching a loudspeaker is set") {
            obj.speakerlabel(atoms{ symbol("M+030") });
            THEN("that loudspeaker gets unity gain") {
                auto gains = last_list(obj, 0);
                REQUIRE(gains.size() == k_channels_050);
                REQUIRE(gains[k_m030] == Approx(1.0));
                REQUIRE(gains[k_m000] == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("only a position is given") {
            obj.speakerlabel(atoms{});
            obj.list(atoms{ 0.0, 0.0 });
            THEN("the channel is panned to the front loudspeaker") {
                auto gains = last_list(obj, 0);
                REQUIRE(gains[k_m000] == Approx(1.0));
            }
        }

        WHEN("the channel is marked as LFE") {
            obj.speakerlabel(atoms{ symbol("LFE1") });
            obj.lfe = true;
            THEN("only the LFE loudspeaker receives signal") {
                auto gains = last_list(obj, 0);
                REQUIRE(gains[k_lfe] == Approx(1.0));
                REQUIRE(gains[k_m000] == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("the layout is switched to 0+2+0") {
            obj.layout = "0+2+0";
            obj.speakerlabel(atoms{ symbol("M+030") });
            THEN("two gains are produced") {
                REQUIRE(obj.channel_count() == 2);
                auto gains = last_list(obj, 0);
                REQUIRE(gains.size() == 2);
                REQUIRE(gains[0] == Approx(1.0));
            }
        }
    }

    GIVEN("an instance created with a layout argument") {
        test_wrapper_args<ear_direct> an_instance(atoms{ symbol("4+5+0") });
        ear_direct& obj = an_instance;

        THEN("the layout argument is honoured") {
            REQUIRE(obj.layout == symbol("4+5+0"));
            REQUIRE(obj.channel_count() == 10);
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(ear_direct, "ear.direct")
