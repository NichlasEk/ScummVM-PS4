#!/usr/bin/env python3
"""Compare a staged GP4 manifest with output from PkgTool.Core pkg_makegp4."""
from pathlib import Path
import hashlib, json, sys, xml.etree.ElementTree as ET
stage, extracted = map(Path, sys.argv[1:])
files=ET.parse(stage/'pkg.gp4').getroot().find('files')
manifest={}
for f in files:
 name=f.attrib['targ_path']
 a=hashlib.sha256((stage/name).read_bytes()).hexdigest()
 b=hashlib.sha256((extracted/name).read_bytes()).hexdigest()
 if a != b: raise SystemExit('Payload mismatch: '+name)
 manifest[name]=a
print(json.dumps({'matching_payloads':len(manifest),'sha256':manifest},indent=2))
