// ============================================================
//  AudioMixer.h — Multi-track audio mixer
// ============================================================
#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <memory>
#include "Timeline.h"
#include "MediaEngine.h"
#include <portaudio.h>

class AudioMixer : public QObject
{
    Q_OBJECT
public:
    static AudioMixer& instance();

    bool   initialize(Timeline* timeline);
    void   shutdown();

    void   play(double fromSeconds = -1.0);
    void   pause();
    void   stop();
    void   seekTo(double seconds);

    bool   isPlaying()  const { return m_playing; }
    double currentTime()const { return m_currentTime; }

    void   setMasterVolume(float v);
    float  masterVolume()  const { return m_masterVolume; }

signals:
    void positionChanged(double seconds);
    void playbackStarted();
    void playbackStopped();

private:
    AudioMixer();
    ~AudioMixer() override;

    static int paCallback(const void* in, void* out,
                          unsigned long frames,
                          const PaStreamCallbackTimeInfo* ti,
                          PaStreamCallbackFlags flags,
                          void* userData);
    int fillBuffer(float* out, unsigned long frames);
    void mixTrack(Track* track, float* buffer, int frames,
                  double startTime, double dt);

    Timeline*           m_timeline     = nullptr;
    PaStream*           m_stream       = nullptr;
    double              m_currentTime  = 0.0;
    double              m_sampleRate   = 44100.0;
    float               m_masterVolume = 1.0f;
    bool                m_playing      = false;
    QMutex              m_mutex;

    // Decoder pool per active audio clip
    QMap<QString, std::unique_ptr<MediaDecoder>> m_decoders;
};
