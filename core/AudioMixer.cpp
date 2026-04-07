// ============================================================
//  AudioMixer.cpp
// ============================================================
#include "AudioMixer.h"
#include <cstring>
#include <cmath>
#include <QDebug>

AudioMixer& AudioMixer::instance()
{
    static AudioMixer inst;
    return inst;
}

AudioMixer::AudioMixer()
{
    Pa_Initialize();
}

AudioMixer::~AudioMixer()
{
    shutdown();
    Pa_Terminate();
}

bool AudioMixer::initialize(Timeline* timeline)
{
    m_timeline = timeline;

    PaStreamParameters out;
    out.device                    = Pa_GetDefaultOutputDevice();
    if (out.device == paNoDevice) {
        qWarning() << "AudioMixer: no output device";
        return false;
    }
    out.channelCount              = 2;
    out.sampleFormat              = paFloat32;
    out.suggestedLatency          =
        Pa_GetDeviceInfo(out.device)->defaultLowOutputLatency;
    out.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&m_stream,
                                nullptr, &out,
                                m_sampleRate, 512,
                                paClipOff,
                                paCallback, this);
    if (err != paNoError) {
        qWarning() << "AudioMixer: Pa_OpenStream failed:" << Pa_GetErrorText(err);
        return false;
    }
    return true;
}

void AudioMixer::shutdown()
{
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
    m_decoders.clear();
}

void AudioMixer::play(double fromSeconds)
{
    if (!m_stream) return;
    QMutexLocker lk(&m_mutex);
    if (fromSeconds >= 0.0) m_currentTime = fromSeconds;
    m_playing = true;
    Pa_StartStream(m_stream);
    emit playbackStarted();
}

void AudioMixer::pause()
{
    QMutexLocker lk(&m_mutex);
    m_playing = false;
    if (m_stream) Pa_StopStream(m_stream);
    emit playbackStopped();
}

void AudioMixer::stop()
{
    QMutexLocker lk(&m_mutex);
    m_playing     = false;
    m_currentTime = 0.0;
    if (m_stream) Pa_StopStream(m_stream);
    emit playbackStopped();
}

void AudioMixer::seekTo(double seconds)
{
    QMutexLocker lk(&m_mutex);
    m_currentTime = seconds;
    // Flush decoders
    for (auto& [id, dec] : m_decoders)
        if (dec) dec->seekTo(seconds);
}

void AudioMixer::setMasterVolume(float v)
{
    m_masterVolume = qBound(0.0f, v, 2.0f);
}

int AudioMixer::paCallback(const void*, void* out,
                           unsigned long frames,
                           const PaStreamCallbackTimeInfo*,
                           PaStreamCallbackFlags,
                           void* userData)
{
    auto* self = static_cast<AudioMixer*>(userData);
    float* buf = static_cast<float*>(out);
    std::memset(buf, 0, frames * 2 * sizeof(float));
    return self->fillBuffer(buf, static_cast<int>(frames));
}

int AudioMixer::fillBuffer(float* buf, int frames)
{
    QMutexLocker lk(&m_mutex);
    if (!m_playing || !m_timeline) return paContinue;

    double dt = frames / m_sampleRate;

    for (auto& trk : m_timeline->tracks()) {
        if (trk->type != Track::TrackType::Audio) continue;
        if (trk->isMuted) continue;
        mixTrack(trk.get(), buf, frames, m_currentTime, dt);
    }

    // Apply master volume
    for (int i = 0; i < frames * 2; ++i)
        buf[i] = qBound(-1.0f, buf[i] * m_masterVolume, 1.0f);

    m_currentTime += dt;
    emit positionChanged(m_currentTime);

    if (m_currentTime >= m_timeline->duration())
        return paComplete;
    return paContinue;
}

void AudioMixer::mixTrack(Track* track, float* buffer, int frames,
                          double startTime, double dt)
{
    auto clip = track->clipAt(startTime);
    if (!clip || clip->isMuted) return;

    // Ensure decoder exists and is positioned
    if (!m_decoders.contains(clip->id)) {
        auto dec = MediaEngine::instance().createDecoder(clip->filePath);
        if (dec->open()) {
            dec->seekTo(startTime - clip->timelineStart + clip->sourceIn);
            m_decoders[clip->id] = std::move(dec);
        }
    }

    auto it = m_decoders.find(clip->id);
    if (it == m_decoders.end()) return;

    auto& dec = it.value();
    AudioFrame af = dec->nextAudioFrame();
    if (!af.isValid) return;

    float vol = static_cast<float>(clip->volume);
    int   n   = qMin(frames * 2, af.samples.size());
    for (int i = 0; i < n; ++i)
        buffer[i] += af.samples[i] * vol;
}
