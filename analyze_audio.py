"""Analyze only the generated synthetic audio verification artifacts."""
import json
from pathlib import Path
import wave

import numpy as np

OUT = Path(__file__).resolve().parent/'verification'
RATE = 48000


def load(path):
    with wave.open(str(path)) as f:
        assert f.getframerate() == RATE and f.getsampwidth() == 2
        return np.frombuffer(f.readframes(f.getnframes()), dtype='<i2').reshape(-1,f.getnchannels()).mean(axis=1)/32768


def compare(reference, observed):
    # FFT cross-correlation at 8 kHz gives sample alignment without quadratic work.
    a, b = reference[::6], observed[::6]
    size = 1 << (len(a)+len(b)-1).bit_length()
    corr = np.fft.irfft(np.fft.rfft(b,size)*np.conj(np.fft.rfft(a,size)),size)
    allowed = np.arange(-8000,16001)
    coarse = int(allowed[np.argmax(corr[allowed % size])])*6
    def dot_at(lag):
        start_x, start_y = max(0,-lag), max(0,lag)
        length = min(len(reference)-start_x,len(observed)-start_y)
        return np.dot(reference[start_x:start_x+length],observed[start_y:start_y+length])
    lag = max(range(coarse-6,coarse+7),key=dot_at)
    if lag >= 0:
        x, y = reference[:len(observed)-lag], observed[lag:]
    else:
        x, y = reference[-lag:], observed[:len(reference)+lag]
    length = min(len(x),len(y))
    x,y = x[:length],y[:length]
    gain = float(np.dot(x,y)/max(np.dot(x,x),1e-20))
    correlation = float(np.dot(x,y)/max(np.linalg.norm(x)*np.linalg.norm(y),1e-20))
    error = float(np.sqrt(np.mean((y-gain*x)**2)))
    return dict(lag_ms=lag/RATE*1000, correlation=correlation,
                fitted_gain=gain, residual_rms=error)


def metrics(a):
    return dict(seconds=len(a)/RATE, rms=float(np.sqrt(np.mean(a*a))),
                peak=float(abs(a).max()), clipped_samples=int(np.sum(abs(a)>=32767/32768)))


def tones(a):
    peaks=[]
    for start,end in [(1.8,2.8),(4.3,5.3)]:
        v=a[int(start*RATE):int(end*RATE)]
        size=2**20
        spectrum=abs(np.fft.rfft(v*np.hanning(len(v)),size))
        peaks.append(float(np.argmax(spectrum)*RATE/size))
    return peaks


results={}
for path in sorted(OUT.glob('*-virtual-mic.wav')):
    if path.stat().st_size <= 44:
        continue
    label=path.name.removesuffix('-virtual-mic.wav')
    mic=load(path)
    result={'virtual_mic':metrics(mic), 'virtual_mic_tone_hz':tones(mic)}
    feed_path=OUT/f'{label}-feed.wav'
    if feed_path.exists():
        feed=load(feed_path)
        result['feed']=metrics(feed)
        result['feed_to_mic']=compare(feed,mic)
    for target in ['rear','front']:
        audio=load(OUT/f'{label}-{target}.wav')
        result[target]=metrics(audio)
        if np.any(audio):
            result[f'mic_to_{target}']=compare(mic,audio)
            result[f'{target}_tone_hz']=tones(audio)
            if label.startswith('discord-') and target == 'rear':
                lag=round(result['mic_to_rear']['lag_ms']*RATE/1000)
                x=mic[7*RATE:16*RATE]
                y=audio[7*RATE+lag:16*RATE+lag]
                count=min(len(x),len(y))//960
                ex=np.sqrt(np.mean(x[:count*960].reshape(-1,960)**2,axis=1))
                ey=np.sqrt(np.mean(y[:count*960].reshape(-1,960)**2,axis=1))
                active=ex>.003
                result['speech_return']={
                    'envelope_correlation_20ms':float(np.corrcoef(ex,ey)[0,1]),
                    'active_windows':int(active.sum()),
                    'silent_return_active_input_windows':int(((ey<.0003)&active).sum()),
                    'mic_interval_s':[7,16]}
    results[label]=result
(OUT/'audio-metrics.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
