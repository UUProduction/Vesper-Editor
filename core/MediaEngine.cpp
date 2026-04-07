// ============================================================
//  MediaEngine.cpp
// ============================================================
#include "MediaEngine.h"
#include <QDebug>
#include <QFileInfo>
#include <cstring>

extern "C" {
#include <libavutil/channel_layout.h>
}

// ── MediaDecoder ─────────────────────────────────────────────
MediaDecoder::MediaDecoder(const QString& filePath)
    : m_filePath(filePath) {}

MediaDecoder::~MediaDecoder() { close(); }

bool MediaDecoder::open()
{
    if (m_opened) return true;

    // Open file
    if (avformat_open_input(&m_fmtCtx,
                            m_filePath.toUtf8().constData(),
                            nullptr, nullptr) < 0) {
        qWarning() << "MediaDecoder: cannot open" << m_filePath;
        return false;
    }
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        qWarning() << "MediaDecoder: no stream info";
        return false;
    }

    probeInfo();

    // Open video codec
    if (m_vidStream >= 0) {
        const AVCodec* vc = avcodec_find_decoder(
            m_fmtCtx->streams[m_vidStream]->codecpar->codec_id);
        if (vc) {
            m_vidCtx = avcodec_alloc_context3(vc);
            avcodec_parameters_to_context(m_vidCtx,
                m_fmtCtx->streams[m_vidStream]->codecpar);
            m_vidCtx->thread_count = 4;
            avcodec_open2(m_vidCtx, vc, nullptr);
        }
    }

    // Open audio codec
    if (m_audStream >= 0) {
        const AVCodec* ac = avcodec_find_decoder(
            m_fmtCtx->streams[m_audStream]->codecpar->codec_id);
        if (ac) {
            m_audCtx = avcodec_alloc_context3(ac);
            avcodec_parameters_to_context(m_audCtx,
                m_fmtCtx->streams[m_audStream]->codecpar);
            avcodec_open2(m_audCtx, ac, nullptr);
            initResampler();
        }
    }

    m_frame  = av_frame_alloc();
    m_packet = av_packet_alloc();
    m_opened = true;
    return true;
}

void MediaDecoder::close()
{
    if (m_frame)  { av_frame_free(&m_frame);   m_frame  = nullptr; }
    if (m_packet) { av_packet_free(&m_packet); m_packet = nullptr; }
    if (m_swsCtx) { sws_freeContext(m_swsCtx); m_swsCtx = nullptr; }
    if (m_swrCtx) { swr_free(&m_swrCtx);       m_swrCtx = nullptr; }
    if (m_vidCtx) { avcodec_free_context(&m_vidCtx); m_vidCtx = nullptr; }
    if (m_audCtx) { avcodec_free_context(&m_audCtx); m_audCtx = nullptr; }
    if (m_fmtCtx) { avformat_close_input(&m_fmtCtx); m_fmtCtx = nullptr; }
    m_opened = false;
}

void MediaDecoder::probeInfo()
{
    m_info.filePath  = m_filePath;
    m_info.duration  = m_fmtCtx->duration / (double)AV_TIME_BASE;
    m_info.bitrate   = m_fmtCtx->bit_rate;
    m_info.format    = m_fmtCtx->iformat->name;

    for (unsigned i = 0; i < m_fmtCtx->nb_streams; ++i) {
        AVStream*           st  = m_fmtCtx->streams[i];
        AVCodecParameters*  par = st->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && m_vidStream < 0) {
            m_vidStream         = static_cast<int>(i);
            m_info.width        = par->width;
            m_info.height       = par->height;
            m_info.frameRate    = av_q2d(st->avg_frame_rate);
            m_info.videoCodec   = avcodec_get_name(par->codec_id);
            m_info.hasVideo     = true;
            ++m_info.videoStreams;
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && m_audStream < 0) {
            m_audStream            = static_cast<int>(i);
            m_info.audioSampleRate = par->sample_rate;
            m_info.audioChannels   = par->ch_layout.nb_channels;
            m_info.audioCodec      = avcodec_get_name(par->codec_id);
            m_info.hasAudio        = true;
            ++m_info.audioStreams;
        }
    }
    m_info.isValid = true;
}

void MediaDecoder::initScaler(int dstW, int dstH)
{
    if (m_swsCtx) sws_freeContext(m_swsCtx);
    m_swsCtx = sws_getContext(
        m_info.width, m_info.height, m_vidCtx->pix_fmt,
        dstW, dstH, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
}

void MediaDecoder::initResampler()
{
    if (!m_audCtx) return;
    swr_alloc_set_opts2(&m_swrCtx,
        &m_audCtx->ch_layout, AV_SAMPLE_FMT_FLTP, 44100,
        &m_audCtx->ch_layout, m_audCtx->sample_fmt,
        m_audCtx->sample_rate,
        0, nullptr);
    swr_init(m_swrCtx);
}

bool MediaDecoder::seekTo(double seconds)
{
    if (!m_fmtCtx) return false;
    int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);
    int ret = av_seek_frame(m_fmtCtx, -1, ts, AVSEEK_FLAG_BACKWARD);
    if (m_vidCtx) avcodec_flush_buffers(m_vidCtx);
    if (m_audCtx) avcodec_flush_buffers(m_audCtx);
    return ret >= 0;
}

QImage MediaDecoder::frameToQImage(AVFrame* f, int dstW, int dstH)
{
    if (!f) return {};
    initScaler(dstW, dstH);
    QImage img(dstW, dstH, QImage::Format_RGB32);
    uint8_t* dst[4]    = { img.bits(), nullptr, nullptr, nullptr };
    int      linesize[4]= { img.bytesPerLine(), 0, 0, 0 };
    sws_scale(m_swsCtx, f->data, f->linesize, 0, f->height, dst, linesize);
    return img;
}

VideoFrame MediaDecoder::nextVideoFrame()
{
    VideoFrame result;
    if (!m_opened || m_vidStream < 0) return result;

    while (av_read_frame(m_fmtCtx, m_packet) >= 0) {
        if (m_packet->stream_index != m_vidStream) {
            av_packet_unref(m_packet);
            continue;
        }
        if (avcodec_send_packet(m_vidCtx, m_packet) == 0) {
            if (avcodec_receive_frame(m_vidCtx, m_frame) == 0) {
                AVRational tb = m_fmtCtx->streams[m_vidStream]->time_base;
                result.pts    = m_frame->pts * av_q2d(tb);
                result.ptsPts = m_frame->pts;
                result.image  = frameToQImage(m_frame, m_info.width, m_info.height);
                result.isValid= true;
                av_packet_unref(m_packet);
                return result;
            }
        }
        av_packet_unref(m_packet);
    }
    return result;
}

AudioFrame MediaDecoder::nextAudioFrame()
{
    AudioFrame result;
    if (!m_opened || m_audStream < 0) return result;

    while (av_read_frame(m_fmtCtx, m_packet) >= 0) {
        if (m_packet->stream_index != m_audStream) {
            av_packet_unref(m_packet);
            continue;
        }
        if (avcodec_send_packet(m_audCtx, m_packet) == 0) {
            if (avcodec_receive_frame(m_audCtx, m_frame) == 0) {
                int    outSamples = swr_get_out_samples(m_swrCtx, m_frame->nb_samples);
                result.channels   = 2;
                result.sampleRate = 44100;
                result.samples.resize(outSamples * 2);

                uint8_t* out = reinterpret_cast<uint8_t*>(result.samples.data());
                swr_convert(m_swrCtx, &out, outSamples,
                            const_cast<const uint8_t**>(m_frame->data),
                            m_frame->nb_samples);

                AVRational tb = m_fmtCtx->streams[m_audStream]->time_base;
                result.pts    = m_frame->pts * av_q2d(tb);
                result.isValid= true;
                av_packet_unref(m_packet);
                return result;
            }
        }
        av_packet_unref(m_packet);
    }
    return result;
}

QImage MediaDecoder::thumbnailAt(double seconds, int width, int height)
{
    if (!open()) return {};
    seekTo(seconds);
    VideoFrame f = nextVideoFrame();
    if (!f.isValid) return {};
    if (f.image.width() == width && f.image.height() == height)
        return f.image;
    return f.image.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QVector<float> MediaDecoder::extractWaveform(int sampleCount)
{
    QVector<float> peaks(sampleCount, 0.0f);
    if (!open() || m_audStream < 0) return peaks;

    seekTo(0);
    double totalDur = m_info.duration;
    if (totalDur <= 0) return peaks;

    QVector<float> allSamples;
    allSamples.reserve(44100 * static_cast<int>(totalDur));

    AudioFrame af;
    while ((af = nextAudioFrame()).isValid)
        allSamples.append(af.samples);

    if (allSamples.isEmpty()) return peaks;

    int stride = qMax(1, allSamples.size() / sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        float peak = 0.f;
        int   base = i * stride;
        for (int j = 0; j < stride && base + j < allSamples.size(); ++j)
            peak = qMax(peak, qAbs(allSamples[base + j]));
        peaks[i] = peak;
    }
    return peaks;
}

// ── MediaEngine ───────────────────────────────────────────────
MediaEngine& MediaEngine::instance()
{
    static MediaEngine inst;
    return inst;
}

MediaEngine::MediaEngine()
{
    avformat_network_init();
}

MediaInfo MediaEngine::probe(const QString& filePath)
{
    MediaDecoder dec(filePath);
    if (!dec.open()) return {};
    MediaInfo info = dec.info();
    dec.close();
    emit probeComplete(filePath, info);
    return info;
}

bool MediaEngine::isSupported(const QString& filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return supportedExtensions().contains(ext);
}

QStringList MediaEngine::supportedExtensions() const
{
    return { "mp4","mov","avi","mkv","webm","m4v",
             "mp3","wav","aac","flac","ogg","m4a",
             "png","jpg","jpeg","bmp","gif","tiff","webp" };
}

std::unique_ptr<MediaDecoder> MediaEngine::createDecoder(const QString& filePath)
{
    return std::make_unique<MediaDecoder>(filePath);
}
