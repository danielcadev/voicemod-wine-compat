# Verification summary

The local software audio path was verified on 2026-10-27 using five calibrated runs of generated tones and speech, approximately 18.09 seconds each. No calls or messages to contacts were made. Saved raw artifacts remain local and are not distributed here.

## Method

Voicemod capture was isolated to a temporary synthetic sink monitor. Four simultaneous recordings measured the actual feed monitor, the Voicemod virtual mic, rear-headphone sink monitor, and front-speaker sink monitor. Each recorder's actual PulseAudio source-output route was checked before playback. Playback volume was explicitly set to unity and Hear Myself remained off.

With effects off, feed and virtual mic were exactly equal after alignment: correlation 1, fitted gain 1, residual RMS 0. Both hardware monitors were silent. This proves real capture without monitoring for the fixture.

Clean Mic loaded twice through its legitimate library tile, with Custom Pitch selected between loads. Both NoReverb and NaturalVoice scan corrections were observed. Read-only native inspection found the SDK's original per-element links and mixer values [0.980000019,0.0199999996] and [0,1] on both loads. Public Bass changes measurably changed the output, then controls were restored. Hidden parameters were not changed.

## Measurements

| Run | Virtual mic RMS | Virtual mic peak | Rear monitor | Front monitor |
| --- | ---: | ---: | --- | --- |
| Effect off | 0.035207 | 0.179993 | Silent | Silent |
| Clean Mic, default controls | 0.106852 | 0.798920 | Silent | Silent |
| Clean Mic, Bass 100 | 0.082600 | 0.785477 | Silent | Silent |
| Discord local test, Clean Mic | 0.106850 | 0.798920 | Matching processed content | Silent |
| Discord local test, Pitch 54 | 0.095988 | 0.795593 | Matching processed content | Silent |

Discord used the virtual mic, explicit rear-headphone output, and Studio input profile. Actual WEBRTC capture and playback routes were checked, not inferred from a meter.

| Voice | Virtual mic peaks | Rear-return peaks | Speech envelope correlation |
| --- | --- | --- | ---: |
| Clean Mic | 440.00 / 880.00 Hz | 439.96 / 880.00 Hz | 0.9965 |
| Custom Pitch 54 | 466.19 / 932.24 Hz | 466.14 / 932.24 Hz | 0.9971 |

Speech envelope analysis used 20 ms RMS windows over seconds 7–16, aligned to the rear return. No silent return windows occurred among 400/395 active speech windows. Active input threshold was RMS 0.003; silent return threshold was 0.0003. No retained recording contained a full-scale clipped sample. Extra Discord local-test delay from virtual mic to rear monitor was approximately 169–171 ms.

Whole-waveform return correlations were approximately 0.813/0.836, so the returned waveform was not bit-identical. These tests establish matching processed content, continuous speech envelope, routing, and fixture clipping behavior. They do not prove perfect subjective fidelity, every voice, physical acoustic performance, or compatibility after an update.

A final local audit passed 27 checks covering signal results, actual stream routes, restoration, unchanged official executable hash, and closed diagnostic port. The original executable SHA-256 remained f62420ebb2452c33e0948b0cb175a44438e32134af2b332bc4c302b7d9497805.

## Restored state and limitations

Clean Mic was left enabled, with Voice Volume 100 and Bass/Mid/Treble 50. Hear Myself was off. Physical capture, existing gain/noise-gate/monitor settings, and global defaults were restored or preserved. Discord's test was stopped and the temporary synthetic sink removed.

Use explicit PulseAudio monitor devices with parecord and verify actual routes. A PulseAudio monitor name passed as a pw-record target can fall back to the default microphone and invalidate the result.

The bridge runs in Wine user space; it does not execute the proprietary Windows kernel driver. Continuous capture lasts while the app is open and is not native client-activity detection. Other voices have not been tested individually. The library's earlier placeholder behavior resolved during normal startup; its exact cause was not established.

## Easy-setup preview

The subsequently added v0.1.0 installer passed nine isolated behavior tests and desktop-entry validation. Its newly compiled components also passed a real fresh-prefix full control-device probe and real audio-session callback probe, using executable-relative DLL paths and an install path containing spaces. System audio defaults remained unchanged. No vendor installer or fresh account login was exercised by that smoke check; that full journey remains unverified. See [installation documentation](INSTALLATION.md).
