"""Local synthetic-only audio checks; never changes the system defaults.

prepare isolates Voicemod from the physical microphone. capture records the
explicit PulseAudio sources and validates their actual source-output routes.
Run cleanup when finished, including after a failed capture.
"""
import argparse
import json
from pathlib import Path
import signal
import subprocess
import time
import wave

import numpy as np

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'verification'
REAR = 'alsa_output.pci-0000_2f_00.4.headphones-rear'
FRONT = 'alsa_output.pci-0000_2f_00.4.creative-front'
PHYSICAL = 'alsa_input.pci-0000_2f_00.4.microphones'
FEED = 'vm_test_feed'


def pactl(*args):
    return subprocess.check_output(['pactl', *map(str, args)], text=True).strip()


def items(kind):
    return json.loads(pactl('-f', 'json', 'list', kind))


def routes():
    sources = {x['index']: x['name'] for x in items('sources')}
    sinks = {x['index']: x['name'] for x in items('sinks')}
    result = {}
    for kind, key, names in [('source-outputs', 'source', sources), ('sink-inputs', 'sink', sinks)]:
        result[kind] = [dict(index=x['index'], app=x['properties'].get('application.name'),
                             media=x['properties'].get('media.name'),
                             target=names.get(x[key]), corked=x['corked'], mute=x['mute'],
                             volume=x['volume'])
                        for x in items(kind)]
    return result


def prepare():
    OUT.mkdir(exist_ok=True)
    state_path = OUT / 'routing-state.json'
    if state_path.exists():
        raise RuntimeError('Existing test state: cleanup first')
    state = dict(default_sink=pactl('get-default-sink'), default_source=pactl('get-default-source'))
    state['module'] = int(pactl('load-module', 'module-null-sink', f'sink_name={FEED}',
                               'rate=48000', 'channels=2', 'sink_properties=device.description=VoicemodSyntheticTest'))
    state_path.write_text(json.dumps(state, indent=2))
    for stream in items('source-outputs'):
        if stream['properties'].get('application.name') == 'Voicemod':
            pactl('move-source-output', stream['index'], FEED+'.monitor')
    current = routes()
    vm = [x for x in current['source-outputs'] if x['app'] == 'Voicemod']
    assert vm and all(x['target'] == FEED+'.monitor' for x in vm), vm
    (OUT / 'isolated-routes.json').write_text(json.dumps(current, indent=2))
    print('Voicemod isolated to synthetic monitor; defaults preserved')
    # Low-level tones with silence markers and speech from the existing synthetic fixture.
    rate = 48000
    def tone(hz, seconds):
        t = np.arange(int(rate*seconds))/rate
        envelope = np.minimum(1, t/.02)*np.minimum(1, (seconds-t)/.02)
        return .10*np.sin(2*np.pi*hz*t)*envelope
    with wave.open(str(ROOT/'compat/discord-speech.wav')) as f:
        assert f.getsampwidth() == 2 and f.getframerate() == rate
        speech = np.frombuffer(f.readframes(f.getnframes()), dtype='<i2').reshape(-1,f.getnchannels()).mean(axis=1)/32768
    speech = speech * (.18/max(abs(speech).max(), .001))
    mono = np.concatenate([np.zeros(rate//2), tone(440,2), np.zeros(rate//2),
                           tone(880,2), np.zeros(rate//2), speech, np.zeros(rate)])
    stereo = np.repeat(mono[:,None], 2, axis=1)
    with wave.open(str(OUT/'synthetic-input.wav'), 'wb') as f:
        f.setparams((2,2,rate,0,'NONE','not compressed'))
        f.writeframes((stereo*32767).astype('<i2').tobytes())


def capture(label):
    assert (OUT/'routing-state.json').exists(), 'prepare first'
    current = routes()
    vm = [x for x in current['source-outputs'] if x['app'] == 'Voicemod']
    assert vm and all(x['target'] == FEED+'.monitor' for x in vm), vm
    recorders = []
    targets = {'feed':FEED+'.monitor', 'virtual-mic':'vm_compat_mic',
               'rear':REAR+'.monitor', 'front':FRONT+'.monitor'}
    try:
        for name, target in targets.items():
            path = OUT/f'{label}-{name}.wav'
            proc = subprocess.Popen(['parecord', f'--device={target}', '--format=s16le',
                                     f'--client-name=VMVerify-{name}',
                                     '--rate=48000', '--channels=2', '--file-format=wav', str(path)])
            recorders.append(proc)
        time.sleep(1)
        actual = routes()
        for name, target in targets.items():
            observed = [x for x in actual['source-outputs'] if x['app'] == f'VMVerify-{name}']
            assert len(observed) == 1 and observed[0]['target'] == target, observed
        (OUT/f'{label}-routes.json').write_text(json.dumps(actual, indent=2))
        playback = subprocess.Popen(['paplay', f'--device={FEED}', '--volume=65536',
                                     '--client-name=VMVerify-feed-playback', str(OUT/'synthetic-input.wav')])
        time.sleep(.5)
        (OUT/f'{label}-playback-routes.json').write_text(json.dumps(routes(), indent=2))
        assert playback.wait(timeout=30) == 0
        time.sleep(2)
    finally:
        for proc in recorders:
            proc.send_signal(signal.SIGINT)
        for proc in recorders:
            proc.wait(timeout=5)
    print(f'Saved {label}: verified virtual-mic, rear monitor, front control')


def cleanup():
    path = OUT/'routing-state.json'
    state = json.loads(path.read_text())
    for stream in items('source-outputs'):
        if stream['properties'].get('application.name') == 'Voicemod':
            pactl('move-source-output', stream['index'], PHYSICAL)
    pactl('unload-module', state['module'])
    assert pactl('get-default-sink') == state['default_sink']
    assert pactl('get-default-source') == state['default_source']
    (OUT/'restored-routes.json').write_text(json.dumps(routes(), indent=2))
    path.rename(OUT/'completed-routing-state.json')
    print('Physical input restored; synthetic sink removed; global defaults unchanged')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['prepare','capture','cleanup'])
    parser.add_argument('label', nargs='?')
    args = parser.parse_args()
    if args.action == 'capture':
        assert args.label and args.label.replace('-','').isalnum()
        capture(args.label)
    else:
        globals()[args.action]()
