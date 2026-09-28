#include "KickVoice.h"

void KickVoice::prepare (double sampleRate)
{
    sub.prepare (sampleRate, fadeSeconds);
    click.prepare (sampleRate);
    fade.prepare (sampleRate, fadeSeconds);
    stop();
}

void KickVoice::start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp, const Settings& s) noexcept
{
    sub.start (pitch, amp, s.sub);
    click.trigger (s.click);
    fade.reset();
}

void KickVoice::fadeOut() noexcept
{
    if (isActive())
        fade.fadeOut();
}

void KickVoice::stop() noexcept
{
    sub.stop();
    click.stop();
}

void KickVoice::render (float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples && isActive(); ++i)
    {
        output[i] += (sub.next() + click.next()) * fade.next();

        if (fade.isFinished())
            stop();
    }
}
