#!/usr/bin/env python3
"""Finite real eventloop/software measurements; does not claim desktop QA."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/qa/aquarium'
OUT.mkdir(parents=True, exist_ok=True)
(ROOT/'build').mkdir(exist_ok=True)
flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'sdl2', 'SDL2_ttf'], text=True).split()
subprocess.run(['cc', '-O2', '-g', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', str(ROOT/'tests/aquarium_driver.c'), str(ROOT/'src/theme.c'), str(ROOT/'src/observe.c'), '-o', str(ROOT/'build/phosphor-aquarium-driver'), *flags, '-lm'], check=True)
results = []
for n in (10000, 50000):
    with tempfile.TemporaryDirectory(prefix='phosphor-aq-loop-') as directory:
        theme = Path(directory) / 'theme.toml'
        fixtures = ROOT / 'tests/fixtures'
        theme.write_bytes((fixtures / 'sulfur-night.toml').read_bytes())
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', XDG_DATA_HOME=directory)
        # stdout goes straight to a file; no slow-consumer measurement distortion.
        with (OUT/f'aquarium-{n}.jsonl').open('w') as output, (OUT/f'aquarium-{n}.err').open('w') as error:
            p = subprocess.Popen([str(ROOT/'build/phosphor-aquarium-driver'), '--size', '1920', '1080', '--points', str(n), '--seconds', '14', '--sample-seconds', '.2', '--status-json', '--theme-file', str(theme), '--log-dir', directory], env=env, stdout=output, stderr=error)
            time.sleep(4)
            theme.write_bytes((fixtures / 'paper-lab.toml').read_bytes())
            assert p.wait(timeout=20) == 0
        records = [json.loads(line) for line in (OUT/f'aquarium-{n}.jsonl').read_text().splitlines()]
        samples = [r['data'] for r in records if r['event'] == 'sample']
        states = [s['state'] for s in samples]
        assert {s['aquarium']['slot'] for s in states if s['aquarium']['active']} == {0,4,9}
        assert any(s['paused'] for s in states)
        assert any(s['width_px']==1100 for s in states)
        events = (Path(directory)/'phosphor-field/events.jsonl').read_text()
        (OUT/f'aquarium-{n}-events.jsonl').write_text(events)
        theme_events = [json.loads(line) for line in events.splitlines() if json.loads(line)['event'] in ('theme_loaded','theme_changed')]
        assert len({e['data']['palette_id'] for e in theme_events})>=2
        assert records[-1]['event']=='exit' and records[-1]['data']['exit_code']==0
        assert not records[-1]['data']['state']['aquarium']['active']
        assert records[-1]['data']['state']['generator']=='pf-medusa'
        assert all(s['invalid']==0 for s in states)
        fades = [s for s in samples if s['state']['aquarium']['active'] and s['state']['aquarium']['fade']<1]
        assert fades
        results.append({'points':n,'backend':'SDL dummy / software','reference':'1920x1080; short resize to 1100x640','fade_samples':len(fades),'fade_mean_work_ms':sum(s['work']['mean_ms'] for s in fades)/len(fades),'fade_max_work_ms':max(s['work']['max_ms'] for s in fades),'fade_mean_fps':sum(s['fps'] for s in fades)/len(fades),'all_fps':records[-1]['data']['all_fps'],'gates':'PASS mixed native/formula, slots 1/5/0, pause, resize, live theme, restore, timed exit'})
(OUT/'aquarium-performance.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
