/// @file
/// @brief   Unit tests for mc.ear.play~ (run against the Min mock kernel): the
///          routing of the file's tracks to the signal outlets, the transport,
///          looping, and the metadata emitted from the audio clock. The reader
///          thread runs for real; the tests wait for it where the audio must
///          be ready.
/// @license Use of this source code is governed by the MIT License found in the License.md file.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <thread>

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
constexpr double k_pi = 3.14159265358979323846;
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

/// a file at `rate` Hz with two tracks of sines (f0 on track 1, f1 on
/// track 2) lasting `frames`, and one object per track whose first block
/// changes at 20 ms
std::string write_tone_fixture(const char* name, uint32_t rate, uint64_t frames, double f0, double f1)
{
    const std::string audio = std::string(EARMAX_TEST_OUT_DIR) + "/" + name + "_audio.wav";
    const std::string out = std::string(EARMAX_TEST_OUT_DIR) + "/" + name + ".wav";
    {
        auto writer = bw64::writeFile(audio, 2, rate, 24);
        std::vector<float> interleaved(static_cast<size_t>(frames) * 2);
        for (size_t f = 0; f < frames; ++f) {
            const double t = static_cast<double>(f) / rate;
            interleaved[f * 2] = static_cast<float>(0.5 * std::sin(2.0 * k_pi * f0 * t));
            interleaved[f * 2 + 1] = static_cast<float>(0.5 * std::sin(2.0 * k_pi * f1 * t));
        }
        writer->write(interleaved.data(), frames);
    }
    admio::captured_object one;
    one.name = "tone";
    admio::object_state s;
    s.azimuth = 30.0;
    one.blocks.push_back({ 0.0, 0.0, s });
    s.azimuth = -30.0;
    one.blocks.push_back({ 0.02, 0.01, s });
    admio::captured_object two;
    two.name = "other";
    two.blocks.push_back({ 0.0, 0.0, s });
    std::vector<bw64::AudioId> chna_ids;
    auto doc = admio::build_document("tones", { one, two }, static_cast<double>(frames) / rate, chna_ids);
    std::remove(out.c_str());
    admio::write_file(out, audio, doc, chna_ids);
    return out;
}

/// the largest deviation of `samples` (from index `from`) from a sine of
/// `freq` Hz with amplitude 0.5 at `rate` Hz
double sine_error(const std::vector<double>& samples, size_t from, double freq, double rate)
{
    double worst = 0.0;
    for (size_t n = from; n < samples.size(); ++n) {
        const double expected = 0.5 * std::sin(2.0 * k_pi * freq * static_cast<double>(n) / rate);
        worst = std::max(worst, std::abs(samples[n] - expected));
    }
    return worst;
}

double rms(const std::vector<double>& samples, size_t from)
{
    double sum = 0.0;
    for (size_t n = from; n < samples.size(); ++n) {
        sum += samples[n] * samples[n];
    }
    return samples.size() > from ? std::sqrt(sum / static_cast<double>(samples.size() - from)) : 0.0;
}

/// a file with one static object and a one-channel bed whose label and
/// position change at 20 ms
std::string write_timed_bed_fixture()
{
    const std::string audio = std::string(EARMAX_TEST_OUT_DIR) + "/timed_bed_audio.wav";
    const std::string out = std::string(EARMAX_TEST_OUT_DIR) + "/timed_bed_fixture.wav";
    {
        auto writer = bw64::writeFile(audio, 2, 48000, 24);
        std::vector<float> interleaved(4800 * 2, 0.25f);
        writer->write(interleaved.data(), 4800);
    }
    admio::captured_programme captured;
    captured.name = "timed bed";
    admio::captured_object object;
    object.name = "still";
    object.blocks.push_back({ 0.0, 0.0, admio::object_state() });
    captured.objects.push_back(object);
    admio::captured_direct_channel ch;
    ch.labels = { "M+030" };
    ch.has_position = true;
    ch.azimuth = 30.0;
    admio::captured_direct_block first;
    first.time = 0.0;
    first.labels = { "M+030" };
    first.has_position = true;
    first.azimuth = 30.0;
    admio::captured_direct_block second = first;
    second.time = 0.02;
    second.labels = { "M-030" };
    second.azimuth = -30.0;
    ch.blocks = { first, second };
    captured.bed.channels.push_back(ch);
    std::vector<bw64::AudioId> chna_ids;
    std::vector<std::string> warnings;
    auto doc = admio::build_document(captured, 0.1, chna_ids, warnings);
    std::remove(out.c_str());
    admio::write_file(out, audio, doc, chna_ids);
    return out;
}

} // namespace

SCENARIO("mc.ear.play~ emits a timed bed's blocks from the audio clock") {
    ext_main(nullptr);

    GIVEN("a file whose bed channel changes at 20 ms") {
        const std::string fixture = write_timed_bed_fixture();
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.open(atoms{ symbol(fixture) });
        REQUIRE(obj.loaded());
        REQUIRE(obj.items().direct.size() == 1);
        REQUIRE(obj.items().direct[0].timed());

        THEN("the first block is sent when the file is opened, with the channel-level parameters") {
            const auto direct = messages(obj, k_direct);
            REQUIRE(contains(direct, "setvalue 1 speakerlabel M+030"));
            REQUIRE(contains(direct, "setvalue 1 position 30 0 1"));
            REQUIRE(contains(direct, "setvalue 1 lfe 0"));
            REQUIRE(count_prefix(direct, "setvalue 1 speakerlabel M-030") == 0);
        }

        const long channels = start_dsp(obj);
        mc_audio_io io(static_cast<size_t>(channels), k_block);

        WHEN("the file plays") {
            obj.start();
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            clear_outputs(obj);
            int emitted_after = -1;
            for (int vector = 1; vector <= 20; ++vector) {
                obj(io.input(), io.output());
                obj.flush();
                if (emitted_after < 0 && contains(messages(obj, k_direct), "setvalue 1 speakerlabel M-030")) {
                    emitted_after = vector;
                }
            }
            THEN("the second block is emitted one vector ahead of frame 960") {
                REQUIRE(emitted_after == 14);
                REQUIRE(contains(messages(obj, k_direct), "setvalue 1 position -30 0 1"));
            }
        }

        WHEN("the transport is moved past the change") {
            obj.start();
            clear_outputs(obj);
            obj.seek(atoms{ 50.0 });
            THEN("the block active there is sent at once") {
                REQUIRE(contains(messages(obj, k_direct), "setvalue 1 speakerlabel M-030"));
            }
        }
    }
}

SCENARIO("the resampler converts interleaved frames between rates") {
    GIVEN("a resampler at the same rate") {
        admio::resampler r;
        r.configure(48000, 48000, 2);
        std::vector<float> in{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
        std::vector<float> out;
        THEN("the input passes through") {
            REQUIRE(r.process(in.data(), 3, out) == 3);
            REQUIRE(out == in);
            REQUIRE(r.history() == 0);
            REQUIRE(r.flush(out) == 0);
        }
    }

    GIVEN("a resampler from 24 kHz to 48 kHz") {
        admio::resampler r;
        r.configure(24000, 48000, 1);
        REQUIRE(r.step() == Approx(0.5));
        REQUIRE(r.history() == 31);
        std::vector<float> in(2400);
        for (size_t n = 0; n < in.size(); ++n) {
            in[n] = static_cast<float>(0.5 * std::sin(2.0 * k_pi * 1000.0 * static_cast<double>(n) / 24000.0));
        }
        std::vector<float> out;
        WHEN("a 1 kHz tone is converted in two pieces and flushed") {
            r.process(in.data(), 1000, out);
            r.process(in.data() + 1000, 1400, out);
            r.flush(out);
            THEN("the output is the tone at 48 kHz, twice as many frames") {
                REQUIRE(out.size() >= 4800);
                REQUIRE(out.size() <= 4800 + 2 * r.history() + 4);
                REQUIRE(r.produced() == out.size());
                // between the band-limited onset and the pre-ringing of the cut at the end
                std::vector<double> samples(out.begin(), out.begin() + 4800 - 128);
                REQUIRE(sine_error(samples, 128, 1000.0, 48000.0) < 2e-3);
            }
        }
        WHEN("a constant is converted") {
            std::fill(in.begin(), in.end(), 0.25f);
            r.process(in.data(), in.size(), out);
            THEN("the output settles at the same level") {
                REQUIRE(out.size() > 200);
                for (size_t n = 100; n < out.size(); ++n) {
                    REQUIRE(out[n] == Approx(0.25).margin(1e-4));
                }
            }
        }
    }

    GIVEN("a resampler from 96 kHz to 48 kHz") {
        admio::resampler r;
        r.configure(96000, 48000, 1);
        REQUIRE(r.step() == Approx(2.0));
        REQUIRE(r.history() == 63);    // the low-pass at 24 kHz needs twice the taps
        std::vector<float> in(9600);
        for (size_t n = 0; n < in.size(); ++n) {
            const double t = static_cast<double>(n) / 96000.0;
            in[n] = static_cast<float>(0.5 * std::sin(2.0 * k_pi * 1000.0 * t) + 0.5 * std::sin(2.0 * k_pi * 30000.0 * t));
        }
        std::vector<float> out;
        r.process(in.data(), in.size(), out);
        THEN("the tone within the band stays and the one above the output's Nyquist is removed") {
            REQUIRE(out.size() >= 4700);
            std::vector<double> samples(out.begin(), out.end());
            REQUIRE(sine_error(samples, 128, 1000.0, 48000.0) < 3e-3);
        }
    }
}

SCENARIO("mc.ear.play~ converts a file at another sample rate to the audio's") {
    ext_main(nullptr);

    GIVEN("a 24 kHz file of 0.2 s playing at 48 kHz") {
        const std::string fixture = write_tone_fixture("tones_24k", 24000, 4800, 1000.0, 2000.0);
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.open(atoms{ symbol(fixture) });
        REQUIRE(obj.loaded());
        const long channels = start_dsp(obj);
        REQUIRE(channels == 4);
        mc_audio_io io(static_cast<size_t>(channels), k_block);
        REQUIRE(obj.wait_ready(k_ready_timeout_ms));

        WHEN("it plays to the end") {
            obj.start();
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            clear_outputs(obj);
            std::vector<double> track1, track2;
            int emitted_after = -1, ended_after = -1;
            for (int vector = 1; vector <= 160; ++vector) {
                obj(io.input(), io.output());
                obj.flush();
                track1.insert(track1.end(), io.outs[0].begin(), io.outs[0].end());
                track2.insert(track2.end(), io.outs[1].begin(), io.outs[1].end());
                if (emitted_after < 0 && count_prefix(messages(obj, k_objects), "setvalue 1 ramp 10") > 0) {
                    emitted_after = vector;
                }
                if (ended_after < 0 && count_prefix(messages(obj, k_info), "end ") > 0) {
                    ended_after = vector;
                }
                if (vector == 75) {
                    REQUIRE(obj.current_time() == Approx(0.1));    // 4800 output frames: 0.1 s of the file
                }
            }
            THEN("the tones come out at 48 kHz, the metadata on the file's clock, and the file ends at its converted length") {
                // up to the file's end (the last frames are the kernel's tail of the cut signal)
                REQUIRE(sine_error(std::vector<double>(track1.begin(), track1.begin() + 9472), 128, 1000.0, k_sr) < 2e-3);
                REQUIRE(sine_error(std::vector<double>(track2.begin(), track2.begin() + 9472), 128, 2000.0, k_sr) < 2e-3);
                // the change at 20 ms is file frame 480 = output frame 960: one vector ahead is vector 14
                REQUIRE(emitted_after == 14);
                REQUIRE(contains(messages(obj, k_objects), "setvalue 1 position -30 0 1"));
                // 4800 file frames become 9600 output frames: 150 vectors (the end may take one more)
                REQUIRE(ended_after >= 150);
                REQUIRE(ended_after <= 152);
                REQUIRE(contains(messages(obj, k_info), "end 200"));
                REQUIRE(rms(track1, 9664) < 1e-3);    // silence after the end
            }
        }

        WHEN("it loops") {
            obj.loop = true;
            obj.start();
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            // the file is exactly 150 vectors at 48 kHz: play up to the last one
            for (int vector = 1; vector <= 149; ++vector) {
                obj(io.input(), io.output());
                obj.flush();
            }
            clear_outputs(obj);
            int emitted_after = -1;
            for (int vector = 0; vector <= 40; ++vector) {    // vector 0 is the file's last: the wrap
                obj(io.input(), io.output());
                obj.flush();
                if (emitted_after < 0 && count_prefix(messages(obj, k_objects), "setvalue 1 ramp 10") > 0) {
                    emitted_after = vector;
                }
            }
            THEN("the second lap starts over as a jump and its block at 20 ms is emitted one vector ahead again") {
                const auto objects = messages(obj, k_objects);
                REQUIRE(objects.front() == "setvalue 1 ramp 0");    // the wrap: the first block again
                REQUIRE(contains(objects, "setvalue 1 position 30 0 1"));
                REQUIRE(emitted_after == 14);    // not at once because the lap counter runs past the file's length
                REQUIRE(obj.current_time() < 0.2);
            }
        }

        WHEN("it is moved into the file") {
            obj.start();
            obj.seek(atoms{ 100.0 });
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            std::vector<double> track1;
            for (int vector = 1; vector <= 20; ++vector) {
                obj(io.input(), io.output());
                track1.insert(track1.end(), io.outs[0].begin(), io.outs[0].end());
            }
            THEN("the output continues the tone from that position without a transient") {
                // 100 ms into a 1 kHz tone is a whole number of cycles: the same sine from the start
                REQUIRE(sine_error(track1, 0, 1000.0, k_sr) < 3e-3);
                REQUIRE(obj.current_time() == Approx(0.1 + 20 * k_block / k_sr));    // time passes at the same rate in both domains
            }
        }
    }

    GIVEN("a 96 kHz file with a tone above the audio's Nyquist") {
        const std::string fixture = write_tone_fixture("tones_96k", 96000, 19200, 1000.0, 30000.0);
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.open(atoms{ symbol(fixture) });
        REQUIRE(obj.loaded());
        const long channels = start_dsp(obj);
        mc_audio_io io(static_cast<size_t>(channels), k_block);
        obj.start();
        REQUIRE(obj.wait_ready(k_ready_timeout_ms));
        std::vector<double> track1, track2;
        for (int vector = 1; vector <= 100; ++vector) {
            obj(io.input(), io.output());
            obj.flush();
            track1.insert(track1.end(), io.outs[0].begin(), io.outs[0].end());
            track2.insert(track2.end(), io.outs[1].begin(), io.outs[1].end());
        }
        THEN("the tone within the band plays and the one above it is filtered out") {
            REQUIRE(sine_error(track1, 256, 1000.0, k_sr) < 3e-3);
            REQUIRE(rms(track2, 256) < 2e-3);
            REQUIRE(obj.current_time() == Approx(100 * k_block * 2.0 / 96000.0));
        }
    }
}

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

                AND_WHEN("'start' is sent again") {
                    clear_outputs(obj);
                    obj.start();
                    // a vector may run before the reader has rewound: the stale end must not stop the restart
                    obj(io.input(), io.output());
                    obj.flush();
                    REQUIRE(obj.playing());
                    REQUIRE(obj.wait_ready(k_ready_timeout_ms));
                    obj(io.input(), io.output());
                    obj.flush();
                    THEN("the file plays from the beginning again") {
                        REQUIRE(obj.playing());
                        REQUIRE(io.outs[0][k_block - 1] == Approx(0.01).margin(1e-6));
                        REQUIRE(count_prefix(messages(obj, k_info), "end ") == 0);
                        REQUIRE(contains(messages(obj, k_objects), "setvalue 1 position 30 0 1"));
                    }
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

        WHEN("a seek follows the open before the reader is ready") {
            obj.open(atoms{ symbol(k_fixture) });
            obj.seek(atoms{ 50.0 });
            obj.resume();
            REQUIRE(obj.wait_ready(k_ready_timeout_ms));
            obj(io.input(), io.output());
            THEN("the audio starts at the seek position, not at the start of the file") {
                REQUIRE(obj.current_time() == Approx(0.05 + k_block / k_sr));
                REQUIRE(io.outs[0][k_block - 1] == Approx(0.01).margin(1e-6));
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

namespace {

/// a one-second file with two tracks: a ramp from -1 to +1 on track 1, a
/// constant 0.25 with one -0.75 sample at frame 24000 on track 2, and one
/// object per track
std::string write_overview_fixture()
{
    const std::string audio = std::string(EARMAX_TEST_OUT_DIR) + "/overview_audio.wav";
    const std::string out = std::string(EARMAX_TEST_OUT_DIR) + "/overview_fixture.wav";
    constexpr uint64_t frames = 48000;
    {
        auto writer = bw64::writeFile(audio, 2, 48000, 24);
        std::vector<float> interleaved(static_cast<size_t>(frames) * 2);
        for (size_t f = 0; f < frames; ++f) {
            interleaved[f * 2] = -1.0f + 2.0f * static_cast<float>(f) / static_cast<float>(frames - 1);
            interleaved[f * 2 + 1] = f == 24000 ? -0.75f : 0.25f;
        }
        writer->write(interleaved.data(), frames);
    }
    admio::captured_object one;
    one.name = "ramp";
    admio::object_state s;
    one.blocks.push_back({ 0.0, 0.0, s });
    admio::captured_object two;
    two.name = "flat";
    two.blocks.push_back({ 0.0, 0.0, s });
    std::vector<bw64::AudioId> chna_ids;
    auto doc = admio::build_document("overview", { one, two }, 1.0, chna_ids);
    std::remove(out.c_str());
    admio::write_file(out, audio, doc, chna_ids);
    return out;
}

bool wait_overview(mc_ear_play_tilde& obj, int timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!obj.overview_ready()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

} // namespace

SCENARIO("the overview of a file holds the lowest and highest sample of every bin") {
    const std::string path = write_overview_fixture();

    GIVEN("the fixture scanned at 10 bins per second") {
        auto reader = bw64::readFile(path);
        const auto ov = admio::compute_overview(*reader, 10.0);

        THEN("it has one bin per 4800 frames and two channels") {
            REQUIRE(ov.samplerate == 48000);
            REQUIRE(ov.channels == 2);
            REQUIRE(ov.bins == 10);
            REQUIRE(ov.bins_per_second == Approx(10.0));
            REQUIRE(ov.buffer_rate() == Approx(20.0));
            REQUIRE(ov.data.size() == 10 * 2 * 2);
        }

        THEN("the ramp's bins span its rise and the flat track keeps its one dip") {
            REQUIRE(ov.minimum(0, 0) == Approx(-1.0).margin(1e-4));
            REQUIRE(ov.maximum(0, 0) == Approx(-0.8).margin(1e-3));
            REQUIRE(ov.minimum(9, 0) == Approx(0.8).margin(1e-3));
            REQUIRE(ov.maximum(9, 0) == Approx(1.0).margin(1e-4));
            for (size_t b = 0; b < 10; ++b) {
                REQUIRE(ov.maximum(b, 1) == Approx(0.25).margin(1e-4));
                REQUIRE(ov.minimum(b, 1) == Approx(b == 5 ? -0.75 : 0.25).margin(1e-4));
            }
        }

        THEN("the layout is a buffer~'s: minima on even frames, maxima on odd frames, channels interleaved") {
            REQUIRE(ov.data[(2 * 5) * 2 + 1] == Approx(-0.75).margin(1e-4));
            REQUIRE(ov.data[(2 * 5 + 1) * 2 + 1] == Approx(0.25).margin(1e-4));
        }
    }

    GIVEN("a bin rate that does not divide the file") {
        auto reader = bw64::readFile(path);
        const auto ov = admio::compute_overview(*reader, 7.0);
        THEN("the last bin takes the remainder") {
            REQUIRE(ov.bins == 7);    // 48000 / (48000 / 7) rounds up to 7 full bins
            REQUIRE(ov.maximum(6, 0) == Approx(1.0).margin(1e-4));
        }
    }

    GIVEN("a scan that is cancelled") {
        auto reader = bw64::readFile(path);
        std::atomic<bool> cancel{ true };
        const auto ov = admio::compute_overview(*reader, 10.0, &cancel);
        THEN("nothing comes back") {
            REQUIRE(ov.bins == 0);
            REQUIRE(ov.data.empty());
        }
    }
}

SCENARIO("mc.ear.play~ scans an overview of the open file on its own thread") {
    ext_main(nullptr);
    const std::string path = write_overview_fixture();

    GIVEN("an instance that opened the fixture") {
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.open(atoms{ symbol(path) });
        REQUIRE(obj.loaded());

        WHEN("an overview is asked for at 10 bins per second") {
            obj.overview(atoms{ symbol("ov"), 10 });
            REQUIRE(wait_overview(obj, 5000));
            obj.flush_overview();    // the main thread's part (the mock kernel has no buffer~, so the write reports)
            THEN("the scan is taken: two tracks, ten bins, the file's samples") {
                const auto& ov = obj.last_overview();
                REQUIRE(ov.channels == 2);
                REQUIRE(ov.bins == 10);
                REQUIRE(ov.minimum(5, 1) == Approx(-0.75).margin(1e-4));
                REQUIRE(!obj.overview_ready());
            }
        }

        WHEN("the bins per second are left out") {
            obj.overview(atoms{ symbol("ov") });
            REQUIRE(wait_overview(obj, 5000));
            obj.flush_overview();
            THEN("100 per second are used") {
                REQUIRE(obj.last_overview().bins == 100);
                REQUIRE(obj.last_overview().buffer_rate() == Approx(200.0));
            }
        }

        WHEN("another file is opened before the result is taken") {
            obj.overview(atoms{ symbol("ov"), 10 });
            REQUIRE(wait_overview(obj, 5000));
            obj.open(atoms{ symbol(write_short_fixture()) });
            obj.flush_overview();
            THEN("the scan of the previous file is dropped") {
                REQUIRE(!obj.overview_ready());
                REQUIRE(obj.last_overview().bins == 0);
            }
        }

        WHEN("the arguments are wrong") {
            obj.overview(atoms{});
            obj.overview(atoms{ symbol("ov"), 0 });
            obj.overview(atoms{ symbol("ov"), symbol("fast") });
            THEN("no scan starts") {
                REQUIRE(!obj.overview_ready());
                REQUIRE(obj.last_overview().bins == 0);
            }
        }
    }

    GIVEN("an instance without a file") {
        test_wrapper<mc_ear_play_tilde> an_instance;
        mc_ear_play_tilde& obj = an_instance;
        obj.overview(atoms{ symbol("ov") });
        THEN("the request is refused") {
            REQUIRE(!obj.overview_ready());
        }
    }
}

EARMAX_TEST_GENERATE_MAXREF(mc_ear_play_tilde, "mc.ear.play~")
