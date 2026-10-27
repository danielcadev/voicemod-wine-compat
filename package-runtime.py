#!/usr/bin/env python3
"""Package only freshly built project components, never the application/prefix."""
import gzip
import hashlib
import importlib.util
import io
from pathlib import Path
import shutil
import tarfile

root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('vmsetup',root/'setup.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
output=root/'dist'
output.mkdir(exist_ok=True)
archive=output/'vmcompat-runtime-x86_64.tar.gz'
with archive.open('wb') as destination, gzip.GzipFile(filename='',mode='wb',fileobj=destination,mtime=0) as compressed:
    with tarfile.open(fileobj=compressed,mode='w') as tar:
        for name in sorted(module.RUNTIME_FILES):
            data=(root/'compat'/name).read_bytes()
            if not data.startswith(b'MZ'):
                raise SystemExit('Expected a compiled project component: '+name)
            private_path=str(Path.home())
            if private_path.encode() in data or private_path.encode('utf-16-le') in data:
                raise SystemExit('Personal build path found in '+name)
            member=tarfile.TarInfo(name)
            member.size=len(data)
            member.mode=0o644
            tar.addfile(member,io.BytesIO(data))
checksum=hashlib.sha256(archive.read_bytes()).hexdigest()
if checksum!=module.RUNTIME_SHA256:
    raise SystemExit('Update setup.py RUNTIME_SHA256 after validating this build: '+checksum)
for name in ['setup.py','install.sh']:
    shutil.copyfile(root/name,output/name)
assets=[archive,output/'setup.py',output/'install.sh']
(output/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in assets))
print(checksum+'  '+str(archive))
