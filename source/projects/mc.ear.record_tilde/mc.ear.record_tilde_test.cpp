/// @file
/// @brief   Unit tests for mc.ear.record~ (run against the Min mock kernel):
///          the audio recorded from the multichannel input, the metadata
///          captured against the audio clock, and the file read back as an
///          ADM file. The writer thread runs for real; the tests wait for it
///          where the file must be finished.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <cstdio>
#include <sstream>

#include "../shared/ear_max_test.h"
#include "mc.ear.record_tilde.h"

#include "mc.ear.record_tilde.cpp"

using namespace earmax_test;

namespace {

// the one message outlet
constexpr int k_info = 0;

constexpr long k_block = 64;
constexpr double k_sr = 48000.0;
constexpr int k_done_timeout_ms = 5000;

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

/// the multichannel input (channel i holds (i + 1) / 4) and no signal output
struct mc_audio_io {
    std::vector<std::vector<double>> ins;
    std::vector<double*> in_ptrs;

    mc_audio_io(size_t inputs, long frames)
    {
        for (size_t ch = 0; ch < inputs; ++ch) {
            ins.emplace_back(static_cast<size_t>(frames), static_cast<double>(ch + 1) / 4.0);
            in_ptrs.push_back(ins.back().data());
        }
    }

    audio_bundle input()
    {
        return audio_bundle{ in_ptrs.data(), static_cast<long>(ins.size()), static_cast<long>(ins[0].size()) };
    }

    audio_bundle output()
    {
        return audio_bundle{ nullptr, 0, static_cast<long>(ins[0].size()) };
    }
};

/// what Max does when the audio starts
void start_dsp(mc_ear_record_tilde& obj)
{
    obj.vector_size(k_block);
    obj.samplerate(k_sr);
    obj.dspsetup(atoms{ k_sr, k_block });
}

std::string out_path(const char* name)
{
    const std::string path = std::string(EARMAX_TEST_OUT_DIR) + "/" + name;
    std::remove(path.c_str());
    return path;
}

/// stop, wait for the writer thread and run the queued report
std::vector<std::string> finish(mc_ear_record_tilde& obj)
{
    obj.stop();
    REQUIRE(obj.wait_done(k_done_timeout_ms));
    obj.flush();
    return messages(obj, k_info);
}

} // namespace

SCENARIO("mc.ear.record~ records the input and the object metadata to an ADM file") {
    ext_main(nullptr);

    GIVEN("an instance recording two objects with the audio running") {
        test_wrapper<mc_ear_record_tilde> an_instance;
        mc_ear_record_tilde& obj = an_instance;
        obj.chans = 2;
        start_dsp(obj);
        REQUIRE(obj.mc_input_changed(0, 2) == 0);

        WHEN("a file is recorded with changes at known positions") {
            const std::string path = out_path("recorded.wav");
            obj.name(atoms{ 1, symbol("mover") });
            obj.setvalue(atoms{ 1, symbol("azimuth"), 30.0 });    // before the start: the first block
            obj.setvalue(atoms{ 2, symbol("azimuth"), 90.0 });
            obj.open(atoms{ symbol(path) });
            obj.start();
            REQUIRE(obj.recording());
            REQUIRE(contains(messages(obj, k_info), "recording " + path));

            mc_audio_io io(2, k_block);
            for (int v = 0; v < 10; ++v) {
                obj(io.input(), io.output());
            }
            obj.setvalue(atoms{ 1, symbol("azimuth"), -30.0 });    // at 640 frames, the default 10 ms ramp
            for (int v = 0; v < 10; ++v) {
                obj(io.input(), io.output());
            }
            obj.setvalue(atoms{ 1, symbol("ramp"), 0 });
            obj.setvalue(atoms{ 1, symbol("elevation"), 15.0 });    // at 1280 frames, a jump
            obj.setvalue(atoms{ 1, symbol("gain"), 0.5 });          // same time: the same block
            for (int v = 0; v < 10; ++v) {
                obj(io.input(), io.output());
            }
            REQUIRE(obj.current_time() == Approx(1920.0 / k_sr));

            const auto info = finish(obj);

            THEN("the file is reported written with its objects and length") {
                REQUIRE(!obj.recording());
                REQUIRE(count_prefix(info, "written " + path + " 2 ") == 1);
                REQUIRE(contains(info, "written " + path + " 2 " + std::to_string(1920.0 / k_sr * 1000.0).substr(0, 2)));
            }

            THEN("the captured timeline has the blocks at the audio's times") {
                const auto& blocks = obj.captured(0);
                REQUIRE(blocks.size() == 3);
                REQUIRE(blocks[0].time == Approx(0.0));
                REQUIRE(blocks[0].state.azimuth == Approx(30.0));
                REQUIRE(blocks[1].time == Approx(640.0 / k_sr));
                REQUIRE(blocks[1].ramp == Approx(0.01));
                REQUIRE(blocks[1].state.azimuth == Approx(-30.0));
                REQUIRE(blocks[2].time == Approx(1280.0 / k_sr));
                REQUIRE(blocks[2].ramp == Approx(0.0));
                REQUIRE(blocks[2].state.elevation == Approx(15.0));
                REQUIRE(blocks[2].state.gain == Approx(0.5));
                REQUIRE(obj.captured(1).size() == 1);
            }

            THEN("the file reads back as an ADM file with the same audio and metadata") {
                auto reader = bw64::readFile(path);
                REQUIRE(reader->channels() == 2);
                REQUIRE(reader->sampleRate() == 48000);
                REQUIRE(reader->bitDepth() == 24);
                REQUIRE(reader->numberOfFrames() == 1920);
                std::vector<float> frames(1920 * 2);
                REQUIRE(reader->read(frames.data(), 1920) == 1920);
                REQUIRE(frames[0] == Approx(0.25).margin(1e-6));
                REQUIRE(frames[1] == Approx(0.5).margin(1e-6));
                REQUIRE(frames[1919 * 2 + 1] == Approx(0.5).margin(1e-6));
                reader.reset();

                const auto file = admio::load_file(path);
                const auto items = admio::select_items(file);
                REQUIRE(items.objects.size() == 2);
                REQUIRE(items.objects[0].name == "mover");
                REQUIRE(items.objects[0].track == 0);
                REQUIRE(items.objects[1].name == "object 2");
                REQUIRE(items.objects[1].track == 1);
                REQUIRE(items.objects[0].blocks.size() == 3);
                REQUIRE(items.objects[0].blocks[1].start == Approx(640.0 / k_sr).margin(1e-6));
                REQUIRE(items.objects[0].blocks[1].interp == Approx(0.01).margin(1e-6));
                REQUIRE(items.objects[0].blocks[2].interp == Approx(0.0));
                REQUIRE(items.objects[0].blocks[1].state.azimuth == Approx(-30.0));
                REQUIRE(items.objects[0].blocks[2].state.elevation == Approx(15.0));
                REQUIRE(items.objects[1].blocks.size() == 1);
                REQUIRE(items.objects[1].blocks[0].state.azimuth == Approx(90.0));
            }
        }

        WHEN("more input channels arrive than objects are recorded") {
            const std::string path = out_path("recorded_extra.wav");
            REQUIRE(obj.mc_input_changed(0, 3) == 0);
            obj.start(atoms{ symbol(path) });
            mc_audio_io io(3, k_block);
            obj(io.input(), io.output());
            finish(obj);
            THEN("only the recorded channels are written") {
                auto reader = bw64::readFile(path);
                REQUIRE(reader->channels() == 2);
                REQUIRE(reader->numberOfFrames() == 64);
            }
        }

        WHEN("fewer input channels arrive than objects are recorded") {
            const std::string path = out_path("recorded_missing.wav");
            obj.start(atoms{ symbol(path) });
            mc_audio_io io(1, k_block);
            obj(io.input(), io.output());
            finish(obj);
            THEN("the missing channel is silent") {
                auto reader = bw64::readFile(path);
                REQUIRE(reader->channels() == 2);
                std::vector<float> frames(64 * 2);
                REQUIRE(reader->read(frames.data(), 64) == 64);
                REQUIRE(frames[0] == Approx(0.25).margin(1e-6));
                REQUIRE(frames[1] == 0.0f);
            }
        }

        WHEN("the file is stopped at once") {
            const std::string path = out_path("recorded_empty.wav");
            obj.start(atoms{ symbol(path) });
            const auto info = finish(obj);
            THEN("an empty file with one block per object is written") {
                REQUIRE(contains(info, "written " + path + " 2 0"));
                const auto items = admio::select_items(admio::load_file(path));
                REQUIRE(items.objects.size() == 2);
                REQUIRE(items.objects[0].blocks.size() == 1);
            }
        }

        WHEN("the file cannot be created") {
            const std::string path = std::string(EARMAX_TEST_OUT_DIR) + "/no_such_folder/recorded.wav";
            obj.start(atoms{ symbol(path) });
            REQUIRE(obj.wait_done(k_done_timeout_ms));
            obj.flush();
            THEN("the failure is reported and the object is idle again") {
                REQUIRE(contains(messages(obj, k_info), "failed " + path));
                REQUIRE(!obj.recording());
            }
        }

        WHEN("there is no file") {
            obj.start();
            THEN("nothing is recorded") {
                REQUIRE(!obj.recording());
                REQUIRE(messages(obj, k_info).empty());
            }
        }

        WHEN("the position is asked while recording") {
            obj.start(atoms{ symbol(out_path("recorded_position.wav")) });
            mc_audio_io io(2, k_block);
            for (int v = 0; v < 3; ++v) {
                obj(io.input(), io.output());
            }
            obj.position();
            THEN("it is the audio recorded so far") {
                REQUIRE(contains(messages(obj, k_info), "position 4"));    // 192 frames = 4 ms
            }
            finish(obj);
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_record_tilde, "mc.ear.record~")
