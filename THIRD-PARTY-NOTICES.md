<!-- SPDX-License-Identifier: MIT -->

# Third-party notices

OpenLF1's own code, scripts, and documentation are MIT-licensed ([LICENSE](LICENSE)).
This file lists the third-party components OpenLF1 links and reproduces their license
notices. The original Little Fighter game, its executable, and its assets are not
redistributed and are not covered here.

When OpenLF1 is built with `OPENLF1_BUNDLE_DEPENDENCIES=ON`, it builds these exact
versions from source. Builds that use system packages link the versions supplied by the
platform, which carry their own copies of the same license terms.

| Component | Version | Purpose | License |
| --- | --- | --- | --- |
| Simple DirectMedia Layer (SDL3) | 3.2.28 or a DJGPP snapshot when bundled, otherwise system-provided | Window, rendering, input, gamepad, and audio support | zlib |
| zlib | 1.3.1 when bundled, otherwise system-provided | ZIP decompression | zlib |

The Linux AppImage also carries the C++ runtime and other shared libraries selected by
linuxdeploy. Those components remain under their own licenses.

## SDL3

```text
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## zlib

```text
Copyright notice:

 (C) 1995-2022 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Jean-loup Gailly        Mark Adler
jloup@gzip.org          madler@alumni.caltech.edu
```
