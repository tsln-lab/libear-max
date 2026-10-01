/// @file
/// @brief   Unit tests for ear.objects (run against the Min mock kernel).
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include "../shared/ear_max_test.h"
#include "ear.objects.h"

EARMAX_TEST_FORWARD_ARGUMENTS(ear_objects)

#include "ear.objects.cpp"

using namespace earmax_test;

namespace {

// 0+5+0 channel order in libear: M+030 M-030 M+000 LFE1 M+110 M-110
constexpr int k_m030 = 0;
constexpr int k_m000 = 2;
constexpr int k_channels_050 = 6;

} // namespace

SCENARIO("ear.objects calculates gains for a BS.2051 layout") {
    ext_main(nullptr);

    GIVEN("an instance created with the default layout") {
        test_wrapper<ear_objects> an_instance;
        ear_objects& obj = an_instance;

        THEN("the layout is 0+5+0 with six channels") {
            REQUIRE(obj.layout == symbol("0+5+0"));
            REQUIRE(obj.channel_count() == k_channels_050);
            REQUIRE(obj.autocalc == true);
        }

        WHEN("a bang is received with the default (front) position") {
            obj.bang();
            THEN("all gain goes to M+000 and the diffuse gains are zero") {
                auto direct = last_list(obj, 0);
                auto diffuse = last_list(obj, 1);
                REQUIRE(direct.size() == k_channels_050);
                REQUIRE(diffuse.size() == k_channels_050);
                REQUIRE(direct[k_m000] == Approx(1.0));
                REQUIRE(direct[k_m030] == Approx(0.0).margin(1e-6));
                for (auto g : diffuse) {
                    REQUIRE(g == Approx(0.0).margin(1e-6));
                }
            }
        }

        WHEN("a list sets the position onto the M+030 loudspeaker") {
            obj.list(atoms{ 30.0, 0.0, 1.0 });
            THEN("the attributes follow and the gains are output automatically") {
                REQUIRE(obj.azimuth == Approx(30.0));
                REQUIRE(obj.elevation == Approx(0.0));
                auto direct = last_list(obj, 0);
                REQUIRE(direct[k_m030] == Approx(1.0));
                REQUIRE(direct[k_m000] == Approx(0.0).margin(1e-6));
            }
        }

        WHEN("the object is fully diffuse") {
            obj.diffuse = 1.0;
            obj.bang();
            THEN("the direct gains are zero and the diffuse gains carry the signal") {
                auto direct = last_list(obj, 0);
                auto diffuse = last_list(obj, 1);
                for (auto g : direct) {
                    REQUIRE(g == Approx(0.0).margin(1e-6));
                }
                REQUIRE(diffuse[k_m000] == Approx(1.0));
            }
        }

        WHEN("channel lock is enabled near a loudspeaker") {
            obj.channellock = true;
            obj.list(atoms{ 14.0, 0.0 });
            THEN("the object snaps to M+000") {
                auto direct = last_list(obj, 0);
                REQUIRE(direct[k_m000] == Approx(1.0));
                REQUIRE(direct[k_m030] == Approx(0.0).margin(1e-9));
            }
        }

        WHEN("divergence is set to 1 with a 30 degree range at the front") {
            obj.divergence_range = 30.0;
            obj.divergence = 1.0;
            obj.bang();
            THEN("the signal is split between M+030 and M-030") {
                auto direct = last_list(obj, 0);
                REQUIRE(direct[k_m030] == Approx(std::sqrt(0.5)));
                REQUIRE(direct[1] == Approx(std::sqrt(0.5)));
                REQUIRE(direct[k_m000] == Approx(0.0).margin(1e-9));
            }
        }

        WHEN("an unsupported feature (cartesian) is enabled") {
            obj.list(atoms{ 30.0, 0.0 });
            auto& output = *c74::max::object_getoutput(obj, 0);
            const auto before = output.size();
            obj.cartesian = true;
            THEN("no new gains are output") {
                REQUIRE(output.size() == before);
            }
        }

        WHEN("the layout attribute is changed to 4+5+0") {
            obj.layout = "4+5+0";
            obj.bang();
            THEN("ten gains are produced") {
                REQUIRE(obj.channel_count() == 10);
                REQUIRE(last_list(obj, 0).size() == 10);
            }
        }

        WHEN("an unknown layout is requested") {
            obj.layout = "not-a-layout";
            THEN("the previous layout is kept") {
                REQUIRE(obj.layout == symbol("0+5+0"));
                REQUIRE(obj.channel_count() == k_channels_050);
            }
        }

        WHEN("the 'channels' message is received") {
            obj.channels();
            THEN("the channel names are sent from the info outlet") {
                auto& output = *c74::max::object_getoutput(obj, 2);
                REQUIRE(output.size() == 1);
                REQUIRE(output[0].size() == k_channels_050 + 1);
                REQUIRE(output[0][0] == symbol("channels"));
                REQUIRE(output[0][1] == symbol("M+030"));
            }
        }
    }

    GIVEN("an instance created with a layout argument") {
        test_wrapper_args<ear_objects> an_instance(atoms{ symbol("4+5+0") });
        ear_objects& obj = an_instance;

        THEN("the layout argument is honoured") {
            REQUIRE(obj.layout == symbol("4+5+0"));
            REQUIRE(obj.channel_count() == 10);
        }
    }
}
