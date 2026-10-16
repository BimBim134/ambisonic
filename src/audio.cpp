// The implementation macros must be defined before the dr_libs headers are
// first included, otherwise their include guards suppress the implementation.
#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION

#include "audio.hpp"

#include <algorithm>
#include <cctype>

namespace sde {

namespace {

std::string lowercase_extension(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return {};
    }
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

}  // namespace

AudioReader::~AudioReader() { close(); }

bool AudioReader::open(const std::string& path) {
    close();
    const std::string ext = lowercase_extension(path);

    if (ext == ".wav") {
        if (!drwav_init_file(&wav_, path.c_str(), nullptr)) {
            return false;
        }
        kind_ = Kind::kWav;
        channels_ = wav_.channels;
        sample_rate_ = wav_.sampleRate;
        total_frames_ = wav_.totalPCMFrameCount;
    } else if (ext == ".flac") {
        flac_ = drflac_open_file(path.c_str(), nullptr);
        if (flac_ == nullptr) {
            return false;
        }
        kind_ = Kind::kFlac;
        channels_ = flac_->channels;
        sample_rate_ = flac_->sampleRate;
        total_frames_ = flac_->totalPCMFrameCount;
    } else if (ext == ".mp3") {
        if (!drmp3_init_file(&mp3_, path.c_str(), nullptr)) {
            return false;
        }
        mp3_open_ = true;
        kind_ = Kind::kMp3;
        channels_ = mp3_.channels;
        sample_rate_ = mp3_.sampleRate;
        total_frames_ = drmp3_get_pcm_frame_count(&mp3_);
    } else {
        return false;
    }
    return true;
}

void AudioReader::close() {
    switch (kind_) {
        case Kind::kWav:
            drwav_uninit(&wav_);
            break;
        case Kind::kFlac:
            if (flac_ != nullptr) {
                drflac_close(flac_);
                flac_ = nullptr;
            }
            break;
        case Kind::kMp3:
            if (mp3_open_) {
                drmp3_uninit(&mp3_);
                mp3_open_ = false;
            }
            break;
        case Kind::kNone:
            break;
    }
    kind_ = Kind::kNone;
    channels_ = 0;
    sample_rate_ = 0;
    total_frames_ = 0;
}

uint64_t AudioReader::read_frames(float* out, uint64_t frames) {
    switch (kind_) {
        case Kind::kWav:
            return drwav_read_pcm_frames_f32(&wav_, frames, out);
        case Kind::kFlac:
            return drflac_read_pcm_frames_f32(flac_, frames, out);
        case Kind::kMp3:
            return drmp3_read_pcm_frames_f32(&mp3_, frames, out);
        case Kind::kNone:
            return 0;
    }
    return 0;
}

AudioWriter::~AudioWriter() { close(); }

bool AudioWriter::open(const std::string& path, uint32_t channels,
                       uint32_t sample_rate) {
    close();
    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = channels;
    format.sampleRate = sample_rate;
    format.bitsPerSample = 16;
    if (!drwav_init_file_write(&wav_, path.c_str(), &format, nullptr)) {
        return false;
    }
    open_ = true;
    return true;
}

bool AudioWriter::write_frames(const int16_t* interleaved, uint64_t frames) {
    if (!open_) {
        return false;
    }
    return drwav_write_pcm_frames(&wav_, frames, interleaved) == frames;
}

void AudioWriter::close() {
    if (open_) {
        drwav_uninit(&wav_);
        open_ = false;
    }
}

}  // namespace sde
