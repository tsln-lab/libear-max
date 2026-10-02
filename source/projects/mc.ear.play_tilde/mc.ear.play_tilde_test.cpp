/// @file
/// @brief   Unit tests for mc.ear.play~ (run against the Min mock kernel): the
///          routing of the file's tracks to the signal outlets, the transport,
///          looping, and the metadata emitted from the audio clock. The reader
///          thread runs for real; the tests wait for it where the audio must
///          be ready.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cstdio>
#include <sstream>

#include "../shared/ear_max_test.h"
#include "mc.ear.play_tilde.h"

#include "mc.ear.play_tilde.cpp"

using namespace earmax_test;

namespace {

const std::string k_fixture = std::string(EARMAX_TEST_DATA_DIR) + "/reference.wav";

// outlets in declaration order: three signal outlets, then the message outlets
enum outlet_index { k_objects = 3, k_direct = 4, k_hoa = 5, k_info = 6 };

constexpr long k_block = 64;
constexpr double k_sr = 48000.0;
constexpr int k_ready_timeout_ms = 5000;

std::string message_text(const c74::max::t_atom_vector& msg)
{
    std::ostringstream s;
    bool first = true;
    for (const auto& a : msg) {
        if (!first) {
            s << ' ';
        }
        first = false;
        if (a.a_type == c74::max::A_SYM) {
            s << a.a_w.w_sym->s_name;
        }
        else if (a.a_type == c74::max::A_LONG) {
            s << a.a_w.w_long;
        }
        else {
            s << a.a_w.w_float;
        }
    }
    return s.str();
}

std::vector<std::string> messages(void* obj, int outlet)
{
    std::vector<std::string> out;
    for (const auto& msg : *c74::max::object_getoutput(obj, outlet)) {
        out.push_back(message_text(msg));
    }
    return out;
}

void clear_outputs(void* obj)
{
    for (int i = k_objects; i <= k_info; ++i) {
        c74::max::object_getoutput(obj, i)->clear();
    }
}

bool contains(const std::vector<std::string>& msgs, const std::string& text)
{
    return std::find(msgs.begin(), msgs.end(), text) != msgs.end();
}

size_t count_prefix(const std::vector<std::string>& msgs, const std::string& prefix)
{
    size_t n = 0;
    for (const auto& m : msgs) {
        if (m.compare(0, prefix.size(), prefix) == 0) {
            ++n;
        }
    }
    return n;
}

/// one dummy input (the object has none) and the output channels of the three signal outlets
struct mc_audio_io {
    std::vector<std::vector<double>> ins;
    std::vector<std::vector<double>> outs;
    std::vector<double*> in_ptrs;
    std::vector<double*> out_ptrs;

    mc_audio_io(size_t outputs, long frames)
        : ins(1, std::vector<double>(frames, 0.0)), outs(outputs, std::vector<double>(frames, 1.0))    // 1.0: silence must be written
    {
        in_ptrs.push_back(ins[0].data());
        for (auto& o : outs) out_ptrs.push_back(o.data());
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

/// what Max does when the audio starts: ask for the outlet channel counts, then dspsetup
long start_dsp(mc_ear_play_tilde& obj)
{
    long total = 0;
    for (long outlet = 0; outlet < 3; ++outlet) {
        total += obj.mc_output_channels(outlet);
    }
    obj.vector_size(k_block);
    obj.samplerate(k_sr);
    obj.dspsetup(atoms{ k_sr, k_block });
    return total;
}

/// a file with two objects whose blocks change within its 0.1 s of audio
std::string write_short_fixture()
{
    const std::string audio = std::string(EARMAX_TEST_OUT_DIR) + "/short_audio.wav";
    const std::string out = std::string(EARMAX_TEST_OUT_DIR) + "/short_fixture.wav";
    {
        auto writer = bw64::writeFile(audio, 2, 48000, 24);
        std::vector<float> interleaved(4800 * 2);
        for (size_t f = 0; f < 4800; ++f) {
            interleaved[f * 2] = 0.25f;
            interleaved[f * 2 + 1] = 0.5f;
        }
        writer->write(interleaved.data(), 4800);
    }
    admio::captured_object one;
    one.name = "mover";
    admio::object_state s;
    s.azimuth = 30.0;
    one.blocks.push_back({ 0.0, 0.0, s });
    s.azimuth = -30.0;
    one.blocks.push_back({ 0.02, 0.01, s });    // at 20 ms, 10 ms ramp
    s.azimuth = 0.0;
    one.blocks.push_back({ 0.05, 0.0, s });     // at 50 ms, a jump
    admio::captured_object two;
    two.name = "still";
    s.azimuth = 90.0;
    two.blocks.push_back({ 0.0, 0.0, s });
    std::vector<bw64::AudioId> chna_ids;
    auto doc = admio::build_document("short", { one, two }, 0.1, chna_ids);
    std::remove(out.c_str());
    admio::write_file(out, audio, doc, chna_ids);
    return out;
}

} // namespace

SCENARIO("mc.ear.play~ plays the tracks of an ADM file to the renderers' outlets") {
    ext_main(nullptr);

    GIVEN("an instance that opened the parity fixture and started the audio") {
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.open(atoms{ symbol(k_fixture) });
        REQUIRE(obj.loaded());
        REQUIRE(obj.items().objects.size() == 2);

        THEN("the file is reported and the static metadata sent, without track lists") {
            const auto info = messages(obj, k_info);
            REQUIRE(count_prefix(info, "file ") == 1);
            REQUIRE(contains(info, "object 1 1 object A 3"));
            REQUIRE(count_prefix(messages(obj, k_direct), "tracks") == 0);
            REQUIRE(contains(messages(obj, k_direct), "setvalue 1 speakerlabel M+030"));
            REQUIRE(contains(messages(obj, k_hoa), "order 1"));
            REQUIRE(count_prefix(messages(obj, k_hoa), "tracks") == 0);
            REQUIRE(contains(messages(obj, k_objects), "setvalue 1 position 30 0 1"));
        }

        THEN("the signal outlets take the item counts of the file") {
            REQUIRE(obj.mc_output_channels(0) == 2);
            REQUIRE(obj.mc_output_channels(1) == 6);
            REQUIRE(obj.mc_output_channels(2) == 4);
            REQUIRE(obj.mc_output_channels(3) == 1);
        }

        const long channels = start_dsp(obj);
        REQUIRE(channels == 12);
        mc_audio_io io(static_cast<size_t>(channels), k_block);

        WHEN("the audio is processed before 'start'") {
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            obj(io.input(), io.output());
            THEN("the outlets are silent") {
                REQUIRE(io.outs[0][0] == 0.0);
                REQUIRE(io.outs[11][k_block - 1] == 0.0);
                REQUIRE(obj.current_time() == Approx(0.0));
            }
        }

        WHEN("the file plays") {
            obj.start();
            REQUIRE(obj.playing());
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            obj(io.input(), io.output());
            THEN("each outlet carries its items' tracks (track i holds the value (i+1)/100)") {
                REQUIRE(io.outs[0][k_block - 1] == Approx(0.01).margin(1e-6));    // object A: track 1
                REQUIRE(io.outs[1][k_block - 1] == Approx(0.02).margin(1e-6));    // object B: track 2
                REQUIRE(io.outs[2][0] == Approx(0.03).margin(1e-6));              // bed M+030: track 3
                REQUIRE(io.outs[7][0] == Approx(0.08).margin(1e-6));              // bed M-110: track 8
                REQUIRE(io.outs[8][0] == Approx(0.09).margin(1e-6));              // scene ACN 0: track 9
                REQUIRE(io.outs[11][0] == Approx(0.12).margin(1e-6));             // scene ACN 3: track 12
                REQUIRE(obj.current_time() == Approx(k_block / k_sr));
            }

            AND_WHEN("it reaches the end of the audio") {
                for (int i = 1; i < 4800 / k_block; ++i) {
                    obj(io.input(), io.output());
                }
                REQUIRE(obj.current_time() == Approx(0.1));
                clear_outputs(obj);
                obj(io.input(), io.output());
                obj.flush();
                THEN("it stops, reports the end and is silent") {
                    REQUIRE_FALSE(obj.playing());
                    REQUIRE(contains(messages(obj, k_info), "end 100"));
                    REQUIRE(io.outs[0][0] == 0.0);
                    obj(io.input(), io.output());
                    REQUIRE(io.outs[8][k_block - 1] == 0.0);
                }
            }

            AND_WHEN("it is paused and resumed") {
                obj.pause();
                REQUIRE_FALSE(obj.playing());
                obj(io.input(), io.output());
                REQUIRE(io.outs[0][0] == 0.0);
                REQUIRE(obj.current_time() == Approx(k_block / k_sr));
                obj.resume();
                obj(io.input(), io.output());
                THEN("the audio continues from where it stopped") {
                    REQUIRE(io.outs[0][k_block - 1] == Approx(0.01).margin(1e-6));
                    REQUIRE(obj.current_time() == Approx(2 * k_block / k_sr));
                }
            }

            AND_WHEN("it is moved with seek") {
                obj.seek(atoms{ 50.0 });
                REQUIRE(obj.wait_ready(k_ready_timeout_ms));
                REQUIRE(obj.current_time() == Approx(0.05));
                obj(io.input(), io.output());
                THEN("playing continues from there") {
                    REQUIRE(obj.playing());
                    REQUIRE(io.outs[0][k_block - 1] == Approx(0.01).margin(1e-6));
                    REQUIRE(obj.current_time() == Approx(0.05 + k_block / k_sr));
                }
            }

            AND_WHEN("the number 0 is sent") {
                obj.number(atoms{ 0 });
                THEN("it stops") {
                    REQUIRE_FALSE(obj.playing());
                }
            }
        }

        WHEN("looping is on and the file plays past its end") {
            obj.loop = true;
            obj.start();
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            for (int i = 0; i < 4800 / k_block; ++i) {
                obj(io.input(), io.output());
                if (i == 4800 / k_block - 2) {
                    // the reader has had time to loop the file into the ring by now
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
            clear_outputs(obj);
            obj(io.input(), io.output());
            obj.flush();
            THEN("the audio continues from the start and the metadata starts over") {
                REQUIRE(obj.playing());
                REQUIRE(io.outs[0][k_block - 1] == Approx(0.01).margin(1e-6));
                REQUIRE(obj.current_time() == Approx(k_block / k_sr));
                const auto objects = messages(obj, k_objects);
                REQUIRE(contains(objects, "setvalue 1 ramp 0"));
                REQUIRE(contains(objects, "setvalue 1 position 30 0 1"));
            }
        }

        WHEN("a file that does not exist is opened") {
            obj.open(atoms{ symbol(std::string(EARMAX_TEST_DATA_DIR) + "/missing.wav") });
            THEN("nothing is loaded and the outlets are silent") {
                REQUIRE_FALSE(obj.loaded());
                REQUIRE(contains(messages(obj, k_info), "file none"));
                obj.start();
                obj(io.input(), io.output());
                REQUIRE(io.outs[0][0] == 0.0);
            }
        }
    }
}

SCENARIO("mc.ear.play~ emits the object metadata from the audio clock") {
    ext_main(nullptr);

    GIVEN("a file whose object changes at 20 ms and 50 ms") {
        const std::string fixture = write_short_fixture();
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.open(atoms{ symbol(fixture) });
        REQUIRE(obj.loaded());
        REQUIRE(obj.items().objects.size() == 2);
        REQUIRE(obj.items().objects[0].blocks.size() == 3);
        const long channels = start_dsp(obj);
        REQUIRE(channels == 4);    // 2 objects, no bed (1 silent channel), no scene (1 silent channel)
        mc_audio_io io(static_cast<size_t>(channels), k_block);

        WHEN("the file plays vector by vector") {
            obj.start();
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            clear_outputs(obj);
            // the change at 20 ms is frame 960: the audio thread wakes the main
            // thread one vector ahead, after vector 14 (896 + 64 >= 960)
            int emitted_after = -1;
            for (int vector = 1; vector <= 20; ++vector) {
                obj(io.input(), io.output());
                obj.flush();
                if (emitted_after < 0 && count_prefix(messages(obj, k_objects), "setvalue 1 ramp 10") > 0) {
                    emitted_after = vector;
                }
            }
            THEN("the block is emitted just before the audio reaches it, with its ramp") {
                REQUIRE(emitted_after == 14);
                const auto objects = messages(obj, k_objects);
                REQUIRE(contains(objects, "setvalue 1 position -30 0 1"));
                REQUIRE(count_prefix(objects, "setvalue 2 ") == 0);    // the still object did not change
                REQUIRE(contains(messages(obj, k_info), "position 20"));
                REQUIRE(io.outs[0][k_block - 1] == Approx(0.25).margin(1e-6));
                REQUIRE(io.outs[1][k_block - 1] == Approx(0.5).margin(1e-6));
                REQUIRE(io.outs[2][k_block - 1] == 0.0);
            }

            AND_WHEN("it goes on to the jump at 50 ms (frame 2400, after vector 37)") {
                clear_outputs(obj);
                int emitted_after = -1;
                for (int vector = 21; vector <= 40; ++vector) {
                    obj(io.input(), io.output());
                    obj.flush();
                    if (emitted_after < 0 && count_prefix(messages(obj, k_objects), "setvalue 1 ") > 0) {
                        emitted_after = vector;
                    }
                }
                THEN("the block is emitted as a jump") {
                    REQUIRE(emitted_after == 37);
                    const auto objects = messages(obj, k_objects);
                    REQUIRE(objects.front() == "setvalue 1 ramp 0");
                    REQUIRE(contains(objects, "setvalue 1 position 0 0 1"));
                }
            }
        }

        WHEN("the transport is moved into the second block") {
            obj.start();
            clear_outputs(obj);
            obj.seek(atoms{ 30.0 });
            THEN("the block active there is emitted at once, as a jump") {
                const auto objects = messages(obj, k_objects);
                REQUIRE(objects.front() == "setvalue 1 ramp 0");
                REQUIRE(contains(objects, "setvalue 1 position -30 0 1"));
                REQUIRE(contains(objects, "setvalue 2 position 90 0 1"));
            }
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_play_tilde, "mc.ear.play~")
