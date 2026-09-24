#!/usr/bin/env python3
"""Apply idempotent Android-only packaging changes to the disposable source tree."""
from pathlib import Path
import shutil, sys
src = Path(sys.argv[1]); ndk = sys.argv[2]
java = src / 'backends/platform/android/org/scummvm/scummvm'
shutil.copy2(Path(__file__).with_name('MobileTools.java'), java / 'MobileTools.java')
p = java / 'ScummVMActivity.java'; s = p.read_text()
anchor = '\t\t_scummvm.setArgs(args);'
if 'MobileTools.install' not in s:
    s = s.replace(anchor, anchor + '\n\t\tMobileTools.install(this, _videoLayout, _scummvm);', 1)
anchor = '\tpublic void onActivityResult(int requestCode, int resultCode, Intent resultData) {'
if 'requestCode == MobileTools.EXPORT' not in s:
    s = s.replace(anchor, anchor + '\n\t\tif (requestCode == MobileTools.EXPORT) {\n\t\t\tif (resultCode == RESULT_OK && resultData != null && resultData.getData() != null) MobileTools.export(this, resultData.getData());\n\t\t\treturn;\n\t\t}')
p.write_text(s)
p = src / 'dists/android/build.gradle'; s = p.read_text()
import re
s = re.sub(r'ndkVersion = "[^"]+"', f'ndkVersion = "{ndk}"', s)
s = s.replace('compileSdk = 35', 'compileSdk = 36').replace('minSdkVersion 16', 'minSdkVersion 26')
s = s.replace('applicationId "org.scummvm.scummvm"', 'applicationId "se.apothic.sotc"')
s = s.replace('versionName "2.9.0"', 'versionName "0.1.0"').replace('versionCode 2091000', 'versionCode 1')
p.write_text(s)
p = src / 'dists/android/res/values/strings.xml'; s = p.read_text().replace('>ScummVM</string>', '>SOTC Preview</string>').replace('>ScummVM (debug)</string>', '>SOTC Preview</string>'); p.write_text(s)
