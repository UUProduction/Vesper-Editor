// ============================================================
//  MediaEngine.h — FFmpeg wrapper
// ============================================================
#pragma once

#include <QString>
#include <QImage>
#include <QVector>
#include <QObject>
#include <functional>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

// ── Media Metadata ────────────────────────────────────────────
struct MediaInfo {
    QString  filePath;
    QString  format;
    double   duration     = 0.0;    // seconds
    int      width        = 0;
    int      height       = 0;
    double   frameRate    = 0.0;
    int      videoStreams = 0;
    int      audioStreams = 0;
    int64_t  bitrate      = 0;
    QString  videoCodec;
    QString  audioCodec;
    int      audioSampleRate = 0;
    int      audioChannels   = 0;
    bool     hasVideo     = false;
    bool     hasAudio     = false;
    bool     isValid      = false;
};

// ── Decoded Frame ─────────────────────────────────────────────
struct VideoFrame {
    QImage   image;          // RGB32
    double   pts = 0.0;      // seconds
    int64_t  ptsPts = 0;     // raw pts
    bool     isValid = false;
};

struct AudioFrame {
    QVector<float> samples;  // interleaved float32
    int            channels  = 2;
    int            sampleRate= 44100;
    double         pts       = 0.0;
    bool           isValid   = false;
};

// ── Decoder ───────────────────────────────────────────────────
class MediaDecoder
{
public:
    explicit MediaDecoder(const QString& filePath);
    ~MediaDecoder();

    bool        open();
    void        close();
    bool        isOpen()  const { return m_opened; }
    MediaInfo   info()    const { return m_info; }

    // Seek to time (seconds)
    bool        seekTo(double seconds);

    // Decode next video frame
    VideoFrame  nextVideoFrame();

    // Decode next audio frame (float PCM, stereo 44100 Hz)
    AudioFrame  nextAudioFrame();

    // Extract thumbnail at a given time
    QImage      thumbnailAt(double seconds, int width = 160, int height = 90);

    // Extract waveform data (normalized 0..1)
    QVector<float> extractWaveform(int sampleCount = 512);

private:
    QString           m_filePath;
    AVFormatContext*  m_fmtCtx     = nullptr;
    AVCodecContext*   m_vidCtx     = nullptr;
    AVCodecContext*   m_audCtx     = nullptr;
    SwsContext*       m_swsCtx     = nullptr;
    SwrContext*       m_swrCtx     = nullptr;
    AVFrame*          m_frame      = nullptr;
    AVPacket*         m_packet     = nullptr;
    int               m_vidStream  = -1;
    int               m_audStream  = -1;
    MediaInfo         m_info;
    bool              m_opened     = false;

    void probeInfo();
    void initScaler(int dstW, int dstH);
    void initResampler();
    QImage frameToQImage(AVFrame* f, int dstW, int dstH);
};

// ── MediaEngine (global manager) ─────────────────────────────
class MediaEngine : public QObject
{
    Q_OBJECT
public:
    static MediaEngine& instance();

    MediaInfo   probe(const QString& filePath);
    bool        isSupported(const QString& filePath) const;
    QStringList supportedExtensions() const;

    // Factory
    std::unique_ptr<MediaDecoder> createDecoder(const QString& filePath);

signals:
    void probeComplete(const QString& filePath, const MediaInfo& info);
    void error(const QString& message);

private:
    MediaEngine();
    ~MediaEngine() override = default;
};
