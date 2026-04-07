// ============================================================
//  ExportEngine.cpp
// ============================================================
#include "ExportEngine.h"
#include "MediaEngine.h"
#include "Renderer.h"
#include <QDebug>
#include <QElapsedTimer>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

ExportEngine::ExportEngine(QObject* parent) : QObject(parent) {}

ExportEngine::~ExportEngine()
{
    cancelExport();
    if (m_thread) m_thread->wait();
}

void ExportEngine::startExport(Timeline* timeline, const ExportSettings& settings)
{
    if (m_exporting) return;
    m_exporting = true;
    m_cancel    = false;

    // Run export in a background thread
    m_thread = QThread::create([this, timeline, settings](){
        doExport(timeline, settings);
    });
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    m_thread->start();
}

void ExportEngine::cancelExport()
{
    m_cancel = true;
}

void ExportEngine::doExport(Timeline* timeline, ExportSettings settings)
{
    // ── Setup output context ──────────────────────────────────
    AVFormatContext* fmtCtx = nullptr;
    const char* outPath = settings.outputPath.toUtf8().constData();

    int ret = avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr, outPath);
    if (ret < 0 || !fmtCtx) {
        emit exportFailed("Failed to allocate output context");
        m_exporting = false;
        return;
    }

    // ── Video codec ──────────────────────────────────────────
    AVCodecID vidCodecId = AV_CODEC_ID_H264;
    if (settings.format == ExportSettings::Format::MP4_H265)
        vidCodecId = AV_CODEC_ID_HEVC;
    else if (settings.format == ExportSettings::Format::GIF)
        vidCodecId = AV_CODEC_ID_GIF;

    const AVCodec* vidCodec = avcodec_find_encoder(vidCodecId);
    if (!vidCodec) {
        avformat_free_context(fmtCtx);
        emit exportFailed("Video codec not found");
        m_exporting = false;
        return;
    }

    AVStream*       vidStream = avformat_new_stream(fmtCtx, vidCodec);
    AVCodecContext* vidCtx    = avcodec_alloc_context3(vidCodec);

    vidCtx->width       = settings.width;
    vidCtx->height      = settings.height;
    vidCtx->time_base   = { 1, static_cast<int>(settings.frameRate * 1000) };
    vidCtx->framerate   = { static_cast<int>(settings.frameRate * 1000), 1000 };
    vidCtx->pix_fmt     = AV_PIX_FMT_YUV420P;
    vidCtx->bit_rate    = settings.videoBitrate;

    switch (settings.quality) {
        case ExportSettings::Quality::Low:      av_opt_set(vidCtx->priv_data, "crf", "28", 0); break;
        case ExportSettings::Quality::Medium:   av_opt_set(vidCtx->priv_data, "crf", "23", 0); break;
        case ExportSettings::Quality::High:     av_opt_set(vidCtx->priv_data, "crf", "18", 0); break;
        case ExportSettings::Quality::Lossless: av_opt_set(vidCtx->priv_data, "crf", "0",  0); break;
    }
    av_opt_set(vidCtx->priv_data, "preset", "medium", 0);

    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        vidCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    avcodec_open2(vidCtx, vidCodec, nullptr);
    avcodec_parameters_from_context(vidStream->codecpar, vidCtx);
    vidStream->time_base = vidCtx->time_base;

    // ── Audio codec ──────────────────────────────────────────
    AVStream*       audStream = nullptr;
    AVCodecContext* audCtx    = nullptr;

    if (settings.includeAudio && settings.format != ExportSettings::Format::GIF) {
        const AVCodec* ac = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (ac) {
            audStream = avformat_new_stream(fmtCtx, ac);
            audCtx    = avcodec_alloc_context3(ac);
            audCtx->sample_rate = settings.audioRate;
            audCtx->bit_rate    = settings.audioBitrate;
            audCtx->sample_fmt  = AV_SAMPLE_FMT_FLTP;
            av_channel_layout_default(&audCtx->ch_layout, 2);
            audCtx->time_base   = { 1, settings.audioRate };
            if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
                audCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            avcodec_open2(audCtx, ac, nullptr);
            avcodec_parameters_from_context(audStream->codecpar, audCtx);
            audStream->time_base = audCtx->time_base;
        }
    }

    // ── Open output file ─────────────────────────────────────
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmtCtx->pb, outPath, AVIO_FLAG_WRITE) < 0) {
            emit exportFailed("Cannot open output file");
            avcodec_free_context(&vidCtx);
            avformat_free_context(fmtCtx);
            m_exporting = false;
            return;
        }
    }
    avformat_write_header(fmtCtx, nullptr);

    // ── Frame loop ───────────────────────────────────────────
    double  totalDur   = timeline->duration();
    double  fps        = settings.frameRate;
    int64_t totalFrames= static_cast<int64_t>(totalDur * fps);
    double  dt         = 1.0 / fps;

    AVFrame*  vidFrame = av_frame_alloc();
    vidFrame->format   = AV_PIX_FMT_YUV420P;
    vidFrame->width    = settings.width;
    vidFrame->height   = settings.height;
    av_frame_get_buffer(vidFrame, 0);

    SwsContext* swsCtx = sws_getContext(
        settings.width, settings.height, AV_PIX_FMT_RGB32,
        settings.width, settings.height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    AVPacket* pkt = av_packet_alloc();
    QElapsedTimer timer;
    timer.start();

    for (int64_t fi = 0; fi < totalFrames && !m_cancel; ++fi) {
        double t = fi * dt;

        // ── Render frame (CPU composite using decoders) ──────
        // In a real pipeline this calls Renderer::renderToImage
        // Here we build a black frame + overlay decoded clips
        QImage rgbFrame(settings.width, settings.height, QImage::Format_RGB32);
        rgbFrame.fill(Qt::black);

        // Composite each visible video track
        for (auto& trk : timeline->tracks()) {
            if (trk->type != Track::TrackType::Video || !trk->isVisible) continue;
            auto clip = trk->clipAt(t);
            if (!clip || clip->isMuted) continue;

            // Decode frame from source
            MediaDecoder dec(clip->filePath);
            if (dec.open()) {
                double srcT = clip->sourceIn + (t - clip->timelineStart);
                QImage f    = dec.thumbnailAt(srcT, settings.width, settings.height);
                if (!f.isNull()) {
                    // Simple composite: just blit (real impl applies transforms)
                    QPainter painter(&rgbFrame);
                    painter.setOpacity(clip->opacity);
                    painter.drawImage(0, 0, f.scaled(settings.width, settings.height,
                                                     Qt::IgnoreAspectRatio,
                                                     Qt::SmoothTransformation));
                }
            }
        }

        // Convert RGB → YUV420P
        const uint8_t* srcData[4] = { rgbFrame.bits(), nullptr, nullptr, nullptr };
        int srcLinesize[4] = { rgbFrame.bytesPerLine(), 0, 0, 0 };
        sws_scale(swsCtx, srcData, srcLinesize, 0, settings.height,
                  vidFrame->data, vidFrame->linesize);
        vidFrame->pts = fi;

        // Encode
        avcodec_send_frame(vidCtx, vidFrame);
        while (avcodec_receive_packet(vidCtx, pkt) == 0) {
            av_packet_rescale_ts(pkt, vidCtx->time_base, vidStream->time_base);
            pkt->stream_index = vidStream->index;
            av_interleaved_write_frame(fmtCtx, pkt);
            av_packet_unref(pkt);
        }

        // Progress
        int pct = static_cast<int>((fi + 1) * 100 / totalFrames);
        double elapsed  = timer.elapsed() / 1000.0;
        double perFrame = elapsed / (fi + 1);
        double eta      = perFrame * (totalFrames - fi - 1);
        emit progressChanged(pct, eta);
    }

    // ── Flush encoder ────────────────────────────────────────
    avcodec_send_frame(vidCtx, nullptr);
    while (avcodec_receive_packet(vidCtx, pkt) == 0) {
        av_packet_rescale_ts(pkt, vidCtx->time_base, vidStream->time_base);
        pkt->stream_index = vidStream->index;
        av_interleaved_write_frame(fmtCtx, pkt);
        av_packet_unref(pkt);
    }

    av_write_trailer(fmtCtx);

    // ── Cleanup ──────────────────────────────────────────────
    sws_freeContext(swsCtx);
    av_frame_free(&vidFrame);
    av_packet_free(&pkt);
    avcodec_free_context(&vidCtx);
    if (audCtx) avcodec_free_context(&audCtx);
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmtCtx->pb);
    avformat_free_context(fmtCtx);

    m_exporting = false;
    if (m_cancel)
        emit exportCancelled();
    else
        emit exportComplete(settings.outputPath);
}
