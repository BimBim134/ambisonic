#pragma once

#include <cstdint>
#include <string>

#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"

namespace sde {

// Streams interleaved float32 frames from a .wav/.flac/.mp3 file.
class AudioReader {
public:
    AudioReader() = default;
    ~AudioReader();
    AudioReader(const AudioReader&) = delete;
    AudioReader& operator=(const AudioReader&) = delete;

    bool open(const std::string& path);
    void close();

    bool is_open() const { return kind_ != Kind::kNone; }
    uint32_t channels() const { return channels_; }
    uint32_t sample_rate() const { return sample_rate_; }
    uint64_t total_frames() const { return total_frames_; }

    // Reads up to `frames` interleaved frames into `out`. Returns the number of
    // frames actually read (0 at end of stream / on error).
    uint64_t read_frames(float* out, uint64_t frames);

private:
    enum class Kind { kNone, kWav, kFlac, kMp3 };

    Kind kind_ = Kind::kNone;
    drwav wav_{};
    drflac* flac_ = nullptr;
    drmp3 mp3_{};
    bool mp3_open_ = false;
    uint32_t channels_ = 0;
    uint32_t sample_rate_ = 0;
    uint64_t total_frames_ = 0;
};

// Writes interleaved PCM16 frames to a .wav file.
class AudioWriter {
public:
    AudioWriter() = default;
    ~AudioWriter();
    AudioWriter(const AudioWriter&) = delete;
    AudioWriter& operator=(const AudioWriter&) = delete;

    bool open(const std::string& path, uint32_t channels, uint32_t sample_rate);
    void close();

    bool write_frames(const int16_t* interleaved, uint64_t frames);

private:
    drwav wav_{};
    bool open_ = false;
};

}  // namespace sde
