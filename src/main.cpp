#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "audio.hpp"
#include "encoder.hpp"
#include "spherical_harmonics.hpp"

namespace fs = std::filesystem;

namespace
{

    constexpr int kChunkSize = 512;
    constexpr double kPcm16Ceil = 1.0 - 1.0 / 32768.0;

    struct FileReport
    {
        std::string input_path;
        std::string out_path;
        bool overwrote = false;
        double preclip_peak = 0.0;
        uint64_t clipped_samples = 0;
        double gain = 0.0;

        bool has_suggested_gain() const { return preclip_peak > 1.0; }
        double suggested_gain() const
        {
            return gain * (kPcm16Ceil / preclip_peak);
        }
    };

    struct ProcessResult
    {
        bool ok = false;
        std::string error;
        FileReport report;
    };

    // libsndfile-compatible float -> PCM16 conversion: clip, scale by 32768 and
    // truncate toward negative infinity (verified to match libsndfile exactly).
    int16_t quantize_pcm16(double x)
    {
        if (x < -1.0)
        {
            x = -1.0;
        }
        else if (x > kPcm16Ceil)
        {
            x = kPcm16Ceil;
        }
        double scaled = x * 32768.0;
        if (scaled < 0.0 && scaled > -1e-10)
        {
            scaled = 0.0;
        }
        scaled = std::floor(scaled);
        if (scaled < -32768.0)
        {
            scaled = -32768.0;
        }
        else if (scaled > 32767.0)
        {
            scaled = 32767.0;
        }
        return static_cast<int16_t>(scaled);
    }

    FileReport process_file(const fs::path& path, const std::string& outdir,
        int chunk_size, int order, double spread, double gain,
        const std::vector<double>& matrix)
    {
        const std::string stem = path.stem().string();
        const fs::path out_path = fs::path(outdir) / (stem + "-ambisonic.wav");
        fs::create_directories(outdir);

        bool overwrote = false;
        if (fs::exists(out_path))
        {
            overwrote = true;
            fs::remove(out_path);
        }

        sde::ProbeEncoder encoder(order, spread, &matrix);
        sde::FixedGain gain_stage(gain);

        sde::AudioReader reader;
        if (!reader.open(path.string()))
        {
            throw std::runtime_error("could not open audio file");
        }
        if (reader.channels() != 2)
        {
            throw std::runtime_error("expected a stereo file, got " +
                std::to_string(reader.channels()) +
                " channel(s)");
        }

        const int num_channels = encoder.num_channels();
        sde::AudioWriter writer;
        if (!writer.open(out_path.string(), static_cast<uint32_t>(num_channels),
            reader.sample_rate()))
        {
            throw std::runtime_error("could not open output file");
        }

        std::vector<float> stereo_buf(
            static_cast<size_t>(chunk_size) * sde::ProbeEncoder::kNumInputChannels);
        std::vector<double> mix_buf(
            static_cast<size_t>(chunk_size) * num_channels);
        std::vector<int16_t> pcm_buf(
            static_cast<size_t>(chunk_size) * num_channels);

        double preclip_peak = 0.0;
        uint64_t clipped_samples = 0;

        while (true)
        {
            const uint64_t frames =
                reader.read_frames(stereo_buf.data(), chunk_size);
            if (frames == 0)
            {
                break;
            }
            const size_t sample_count = static_cast<size_t>(frames) * num_channels;
            encoder.process_block(stereo_buf.data(), static_cast<size_t>(frames),
                mix_buf.data());
            gain_stage.process(mix_buf.data(), sample_count);

            for (size_t i = 0; i < sample_count; ++i)
            {
                const double v = mix_buf[i];
                const double a = std::fabs(v);
                if (a > preclip_peak)
                {
                    preclip_peak = a;
                }
                if (a > 1.0)
                {
                    ++clipped_samples;
                }
            }
            for (size_t i = 0; i < sample_count; ++i)
            {
                pcm_buf[i] = quantize_pcm16(mix_buf[i]);
            }
            if (!writer.write_frames(pcm_buf.data(), frames))
            {
                throw std::runtime_error("failed writing output");
            }
        }

        FileReport report;
        report.input_path = path.string();
        report.out_path = out_path.string();
        report.overwrote = overwrote;
        report.preclip_peak = preclip_peak;
        report.clipped_samples = clipped_samples;
        report.gain = gain;
        return report;
    }

    void announce(const FileReport& report)
    {
        const std::string out_name = fs::path(report.out_path).filename().string();
        if (report.overwrote)
        {
            std::cout << "overwriting " << report.out_path << "\n";
        }
        std::cout << "Saved " << out_name << "\n";
        if (report.has_suggested_gain())
        {
            std::cout << std::fixed << std::setprecision(2) << "warning: "
                << out_name << " clipped (" << report.clipped_samples
                << " samples, peak " << report.preclip_peak << "); try --gain "
                << report.suggested_gain() << " or lower\n";
        }
        std::cout.flush();
    }

    const std::vector<std::string>& audio_extensions()
    {
        static const std::vector<std::string> exts = { ".wav", ".flac", ".mp3" };
        return exts;
    }

    std::string lower_copy(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    bool has_audio_extension(const fs::path& p)
    {
        const std::string ext = lower_copy(p.extension().string());
        const auto& exts = audio_extensions();
        return std::find(exts.begin(), exts.end(), ext) != exts.end();
    }

    std::vector<fs::path> find_audio_files(const fs::path& path)
    {
        std::vector<fs::path> result;
        std::error_code ec;
        if (fs::is_regular_file(path, ec))
        {
            if (has_audio_extension(path))
            {
                result.push_back(path);
            }
            return result;
        }
        if (fs::is_directory(path, ec))
        {
            // Deduplicate case-insensitively (Windows semantics) and sort by the
            // lowercased file name.
            std::vector<std::pair<std::string, fs::path>> found;
            for (const auto& entry : fs::directory_iterator(path, ec))
            {
                if (!entry.is_regular_file() || !has_audio_extension(entry.path()))
                {
                    continue;
                }
                found.emplace_back(lower_copy(entry.path().filename().string()),
                    entry.path());
            }
            std::sort(found.begin(), found.end(),
                [](const auto& a, const auto& b)
                { return a.first < b.first; });
            std::string last;
            bool first = true;
            for (const auto& item : found)
            {
                if (first || item.first != last)
                {
                    result.push_back(item.second);
                }
                last = item.first;
                first = false;
            }
        }
        return result;
    }

    void dump_matrix(int order, double spread)
    {
        const auto matrix = sde::build_encoder_matrix(order, spread);
        const int num_channels = (order + 1) * (order + 1);
        std::cout << std::setprecision(17);
        std::cout << "order " << order << "\n";
        std::cout << "channels " << num_channels << "\n";
        for (int r = 0; r < 2; ++r)
        {
            for (int c = 0; c < num_channels; ++c)
            {
                std::cout << matrix[static_cast<size_t>(r * num_channels + c)]
                    << "\n";
            }
        }
    }

    void print_usage(const char* prog)
    {
        std::cout
            << "usage: " << prog << " <input> [options]\n\n"
            << "Encode stereo audio into an SN3D ambisonic mix, processing the\n"
            << "file in 512-sample blocks.\n\n"
            << "options:\n"
            << "  -g, --gain FLOAT        fixed (signal-independent) output gain "
            "(default 0.1)\n"
            << "  -o, --order INT         ambisonic order (default 3)\n"
            << "  -s, --spread FLOAT      virtual-source azimuth spread "
            "(default 1.5)\n"
            << "  -c, --chunk-size INT    block size in samples (default 512)\n"
            << "      --outdir DIR        output folder (default OUT)\n"
            << "  -j, --jobs INT          number of worker threads "
            "(default 0 = all cores)\n"
            << "      --dump-matrix       print the encoder matrix and exit\n";
    }

    struct Args
    {
        std::string input;
        double gain = sde::kDefaultGain;
        int order = sde::kDefaultOrder;
        double spread = sde::kDefaultSpread;
        int chunk_size = kChunkSize;
        std::string outdir = "OUT";
        int jobs = 0;
        bool dump_matrix = false;
        bool help = false;
    };

    Args parse_args(int argc, char** argv)
    {
        Args args;
        std::vector<std::string> positional;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            auto next_value = [&](const char* name) -> std::string
                {
                    if (i + 1 >= argc)
                    {
                        throw std::runtime_error(std::string("missing value for ") +
                            name);
                    }
                    return argv[++i];
                };
            if (arg == "-h" || arg == "--help")
            {
                args.help = true;
            }
            else if (arg == "-g" || arg == "--gain")
            {
                args.gain = std::stod(next_value("--gain"));
            }
            else if (arg == "-o" || arg == "--order")
            {
                args.order = std::stoi(next_value("--order"));
            }
            else if (arg == "-s" || arg == "--spread")
            {
                args.spread = std::stod(next_value("--spread"));
            }
            else if (arg == "-c" || arg == "--chunk-size")
            {
                args.chunk_size = std::stoi(next_value("--chunk-size"));
            }
            else if (arg == "--outdir")
            {
                args.outdir = next_value("--outdir");
            }
            else if (arg == "-j" || arg == "--jobs")
            {
                args.jobs = std::stoi(next_value("--jobs"));
            }
            else if (arg == "--dump-matrix")
            {
                args.dump_matrix = true;
            }
            else if (!arg.empty() && arg[0] == '-')
            {
                throw std::runtime_error("unknown option: " + arg);
            }
            else
            {
                positional.push_back(arg);
            }
        }
        if (!positional.empty())
        {
            args.input = positional.front();
        }
        return args;
    }

} // namespace

int main(int argc, char** argv)
{
    Args args;
    try
    {
        args = parse_args(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }

    if (args.help)
    {
        print_usage(argv[0]);
        return 0;
    }

    if (args.dump_matrix)
    {
        dump_matrix(args.order, args.spread);
        return 0;
    }

    if (args.input.empty())
    {
        std::cerr << "Error: no input given\n";
        print_usage(argv[0]);
        return 2;
    }

    const std::vector<fs::path> audio_files = find_audio_files(args.input);
    if (audio_files.empty())
    {
        std::cout << "No audio files found in " << args.input << "\n";
        return 1;
    }

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0)
    {
        hw = 1;
    }
    int jobs = args.jobs > 0 ? args.jobs : static_cast<int>(hw);
    jobs = std::max(1, std::min(jobs, static_cast<int>(audio_files.size())));

    // Build the constant encoder matrix once and share it with the workers.
    const std::vector<double> matrix =
        sde::build_encoder_matrix(args.order, args.spread);

    if (jobs == 1)
    {
        for (const auto& file : audio_files)
        {
            std::cout << "Processing " << file.string() << "...\n";
            try
            {
                const FileReport report = process_file(
                    file, args.outdir, args.chunk_size, args.order, args.spread,
                    args.gain, matrix);
                announce(report);
            }
            catch (const std::exception& e)
            {
                std::cout << "Error processing " << file.string() << ": "
                    << e.what() << "\n";
            }
        }
        return 0;
    }

    std::cout << "Processing " << audio_files.size() << " file(s) on " << jobs
        << " workers...\n";
    std::atomic<size_t> next_index{ 0 };
    std::mutex io_mutex;
    std::vector<std::thread> workers;
    for (int w = 0; w < jobs; ++w)
    {
        workers.emplace_back([&]()
            {
                while (true) {
                    const size_t index = next_index.fetch_add(1);
                    if (index >= audio_files.size()) {
                        break;
                    }
                    const fs::path& file = audio_files[index];
                    ProcessResult result;
                    try {
                        result.report = process_file(
                            file, args.outdir, args.chunk_size, args.order,
                            args.spread, args.gain, matrix);
                        result.ok = true;
                    }
                    catch (const std::exception& e) {
                        result.ok = false;
                        result.error = e.what();
                    }
                    std::lock_guard<std::mutex> lock(io_mutex);
                    if (result.ok) {
                        announce(result.report);
                    }
                    else {
                        std::cout << "Error processing " << file.string() << ": "
                            << result.error << "\n";
                        std::cout.flush();
                    }
                } });
    }
    for (auto& worker : workers)
    {
        worker.join();
    }
    return 0;
}
