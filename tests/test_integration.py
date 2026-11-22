#!/usr/bin/env python3
"""Operational tests run only against private temporary paths; never switch the desktop."""
import json, os, pathlib, shutil, signal, subprocess, tempfile, time, threading
ROOT=pathlib.Path(__file__).resolve().parents[1]
APP=str(ROOT/'phosphor-field')
OUT=pathlib.Path(os.environ.get('PHOSPHOR_QA_DIR',str(ROOT/'build/qa/integration')))
OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,SDL_VIDEODRIVER='dummy')
VERSION=subprocess.run([APP,'--version'],capture_output=True,text=True,check=True).stdout.split()[1]
checks=[]
def decode(raw):
    records=[json.loads(line) for line in raw.splitlines()]
    assert records and all(r['schema']==1 and r['program']=='phosphor-field' and r['version']==VERSION for r in records)
    return records

def run(args,env=ENV,code=0):
    p=subprocess.run([APP,'--status-json',*args],env=env,text=True,capture_output=True,timeout=20)
    assert p.returncode==code,(args,p.returncode,p.stderr)
    return decode(p.stdout),p.stderr

def check_exit(records):
    final=records[-1];assert final['event']=='exit' and final['data']['exit_code']==0
    data=final['data'];all_work=data['all']
    assert all_work['frames']==sum(all_work['bins'])
    assert all_work['frames']==data['warmup']['frames']+data['steady']['frames']+1
    assert data['state']['text_live']==0
    return data

def wait_ready(p):
    # Every process has an independent 10-second deadline, including renderer initialization.
    import selectors
    with selectors.DefaultSelector() as selector:
        selector.register(p.stdout,selectors.EVENT_READ)
        deadline=time.monotonic()+10
        records=[]
        while time.monotonic()<deadline:
            assert p.poll() is None,'exited before ready'
            if selector.select(.1):
                record=json.loads(p.stdout.readline());records.append(record)
                if record['event']=='ready':return records
    p.kill();raise AssertionError('ready timeout')

with tempfile.TemporaryDirectory(prefix='phosphor-integration-') as temporary:
    tmp=pathlib.Path(temporary)
    records,_=run(['--seconds','.6','--theme','phosphor','--sample-seconds','.1'])
    check_exit(records);assert any(r['event']=='sample' for r in records);checks.append('JSON schema, histogram, sampling, ready/exit, released cache')
    records,_=run(['--seconds','.2','--theme-file',str(tmp/'missing.toml')]);check_exit(records)
    assert records[0]['data']['theme_valid'] is False;checks.append('missing palette fallback')
    fifo=tmp/'fifo-palette';os.mkfifo(fifo)
    records,_=run(['--seconds','.2','--theme-file',str(fifo)]);check_exit(records)
    assert records[0]['data']['theme_valid'] is False;checks.append('non-regular palette falls back without blocking')
    base=tmp/'logs';base.mkdir();foreign=base/'events.jsonl';foreign.write_text('foreign log\n')
    records,_=run(['--seconds','1.2','--theme','phosphor','--qa','--sample-seconds','.1','--log-max-bytes','4096','--log-dir',str(base)])
    check_exit(records);assert foreign.read_text()=='foreign log\n'
    files=list((base/'phosphor-field').glob('events*.jsonl'));assert len(files)==3
    event_names=set()
    for f in files:
        assert f.stat().st_size<=4096
        event_names.update(r['event'] for r in decode(f.read_text()))
    assert 'exit' in event_names;checks.append('bounded rotation and foreign files preserved')
    # A pre-existing unmarked directory must be left entirely untouched.
    bad=tmp/'foreign';(bad/'phosphor-field').mkdir(parents=True)
    sentinel=bad/'phosphor-field/events.jsonl';sentinel.write_text('DO NOT MODIFY')
    records,stderr=run(['--seconds','.2','--log-dir',str(bad)])
    check_exit(records);assert sentinel.read_text()=='DO NOT MODIFY' and stderr.count('file logging disabled')==1
    checks.append('foreign/unmarked log namespace refused, renderer continues')
    os.mkfifo(bad/'phosphor-field/.owner')
    records,stderr=run(['--seconds','.2','--log-dir',str(bad)])
    check_exit(records);assert sentinel.read_text()=='DO NOT MODIFY'
    checks.append('non-regular owner marker is refused without blocking')
    # Symlink files must never redirect file logging.
    (base/'phosphor-field/events.jsonl').unlink();(base/'phosphor-field/events.jsonl').symlink_to(foreign)
    records,stderr=run(['--seconds','.2','--log-dir',str(base)])
    check_exit(records);assert foreign.read_text()=='foreign log\n' and stderr.count('file logging disabled')==1
    checks.append('symlink log target refused')
    notdir=tmp/'not-a-directory';notdir.write_text('sentinel')
    records,stderr=run(['--seconds','.2','--log-dir',str(notdir)])
    check_exit(records);assert stderr.count('file logging disabled')==1;checks.append('unwritable destination continues without log flood')
    for sig in [signal.SIGTERM,signal.SIGINT]:
        p=subprocess.Popen([APP,'--status-json','--theme','phosphor'],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        first=wait_ready(p);p.send_signal(sig);raw,stderr=p.communicate(timeout=10)
        data=check_exit(first+decode(raw));assert p.returncode==0 and data['reason']=='signal' and data['signal']==sig
        checks.append(f'orderly {sig.name}')
    p=subprocess.Popen([APP,'--status-json','--seconds','.3','--theme','phosphor'],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    p.stdout.close();p.stdout=None
    _,stderr=p.communicate(timeout=10);assert p.returncode==0 and stderr.count('status output unavailable')==1
    checks.append('closed status pipe does not terminate animation/shutdown')
    slow=tmp/'slow-consumer'
    p=subprocess.Popen([APP,'--status-json','--seconds','1.5','--sample-seconds','.1','--theme','phosphor','--log-dir',str(slow)],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,pipesize=4096)
    p.wait(timeout=10) # Deliberately never drain stdout until the process has finished.
    raw=p.stdout.read();stderr=p.stderr.read();decode(raw)
    assert p.returncode==0
    events=decode((slow/'phosphor-field/events.jsonl').read_text())
    assert events[-1]['event']=='exit' and events[-1]['data']['status_dropped']>0
    assert stderr.count('slow status consumer')==1
    checks.append('slow status pipe drops complete records without blocking rendering or corrupting JSON')
    records,stderr=run(['--seconds','.2'],dict(ENV,SDL_VIDEODRIVER='invalid'),1)
    assert records[-1]['event']=='exit' and records[-1]['data']['exit_code']==1 and not any(r['event']=='ready' for r in records)
    checks.append('initialization failure has no false ready and exits 1')
    for args in [['--seconds','nan'],['--points','123abc'],['--size','320.5','240'],['--sample-seconds','0'],['--theme','garbage']]:
        p=subprocess.run([APP,*args],env=ENV,text=True,capture_output=True)
        assert p.returncode==1
    checks.append('strict CLI validation')
    # Exercise the installed Omarchy directory/name mechanism using an app-specific path override.
    state=tmp/'state';current=state/'omarchy/current';current.mkdir(parents=True)
    themes=ROOT/'tests/fixtures'
    def install(name):
        staged=current/'next';staged.mkdir()
        shutil.copyfile(themes/f'{name}.toml',staged/'colors.toml')
        old=current/'theme'
        if old.exists():shutil.rmtree(old)
        staged.rename(old);(current/'theme.name').write_text(name+'\n')
    install('sulfur-night')
    env=dict(ENV,SDL_VIDEODRIVER='offscreen')
    live=tmp/'live';p=subprocess.Popen([APP,'--status-json','--theme-current',str(current),'--seconds','11','--sample-seconds','.5','--log-dir',str(live)],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    first=wait_ready(p);assert first[-1]['data']['theme']=='sulfur-night'
    live_records=[]
    reader=threading.Thread(target=lambda:live_records.extend(json.loads(line) for line in p.stdout));reader.start()
    install('paper-lab');time.sleep(2.4)
    (current/'theme/colors.toml').write_text('accent = "bad"\n');time.sleep(1.3)
    install('sulfur-night');time.sleep(2.4)
    p.wait(timeout=15);reader.join(timeout=2);stderr=p.stderr.read();records=first+live_records;data=check_exit(records)
    assert not reader.is_alive() and p.returncode==0 and data['status_dropped']==0
    assert data['state']['theme']=='sulfur-night' and data['state']['phase']>10
    log_records=decode((live/'phosphor-field/events.jsonl').read_text())
    events=[r['event'] for r in log_records]
    assert 'theme_loaded' in events and 'theme_changed' in events and 'theme_retained' in events and 'theme_recovered' in events
    samples=[r['data'] for r in records if r['event']=='sample']
    assert any(r['state']['theme']=='paper-lab' for r in samples)
    assert all(b['state']['phase']>a['state']['phase'] for a,b in zip(samples,samples[1:]))
    (OUT/'theme-live-status.jsonl').write_text('\n'.join(json.dumps(r) for r in records)+'\n')
    (OUT/'theme-live-events.jsonl').write_text('\n'.join(json.dumps(r) for r in log_records)+'\n')
    checks.append('dark/light fixture palettes: live directory swap, invalid retention, return, animation advances')
OUT.mkdir(parents=True,exist_ok=True)
(OUT/'integration.json').write_text(json.dumps({'passed':checks},indent=2)+'\n')
print('PASS:',len(checks),'operational integration checks')
for check in checks:print(' -',check)
