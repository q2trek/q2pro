Q2PRO
=====

Tick rates from 10 to 60 Hz in this fork
----------------------------------------

This is q2trek/q2pro. Its `tickrate` branch is upstream `master` plus the
tick rate commits, and `master` follows upstream unchanged.

Upstream Q2PRO already runs `sv_fps` above 10 when the engine is built with
the `variable-fps` meson option (off by default, on in the nightly builds) and
the game library sets `GMF_VARIABLE_FPS`. The same option is needed here. The
bundled baseq2 doesn't set the flag, but OpenFFA and OpenTDM do (see
q2trek/openffa and q2trek/opentdm). Upstream keeps the frame period in whole
milliseconds, though. At 60 Hz that's 16 ms, so the server makes 62.5 steps
per second while the game counts each step as 1/60 s, and the world runs about
4% fast (1% at 30 Hz). This branch counts frame times exactly at every rate
from 10 to 60 Hz, on the server and in the client (see `inc/common/utils.h`).

Clients and servers can be mixed. On a server of this fork:

* a client of this fork gets the full rate
* an upstream Q2PRO client gets the highest rate that divides the server rate
  and has a whole millisecond period: 20 Hz from a 60 Hz server, 10 Hz from a
  30 Hz one, and the full rate at 10, 20, 40 and 50 Hz (with 16 ms frames it
  would fall 40 ms behind a 60 Hz server every second and have to jump)
* clients without the Q2PRO protocol get 10 Hz, as before

On an upstream server, a client of this fork behaves like an upstream client.

The client asks for the exact timeline by sending the setting `CLS_EXACTFPS`
(index 0x4458, value 1) right before `CLS_FPS`, and the server answers with
`SVS_EXACTFPS 1` before `SVS_FPS`. Upstream servers drop the unknown index,
upstream clients ignore it, and the protocol version doesn't change. Demos
carry no settings (they're recorded with protocol 34 or its extended variant),
so playback keeps the old timeline. Demo and MVD formats don't change.

Q2PRO is an enhanced Quake 2 client and server for Windows and Linux. Supported
features include:

* unified OpenGL renderer with support for wide range of OpenGL versions
* enhanced console with persistent command history and autocompletion
* rendering / physics / packet rate separation
* syncing to GPU for reduced input lag with vsync on
* ZIP packfiles (.pkz)
* JPEG/PNG textures and screenshots
* MD3 and MD5 (re-release) models
* Ogg Vorbis music and Ogg Theora cinematics
* compatibility with re-release assets
* fast and secure HTTP downloads
* multichannel sound using OpenAL
* stereo WAV files support
* seeking in demos, recording from demos, server side multiview demos
* live game broadcasting capabilities
* network protocol extensions for larger maps
* eliminates frame overflows (even for legacy clients)
* won't crash if game data is corrupted

Q2PRO doesn't have releases. It is always recommended to use the latest nighly
build from the top of Releases page.

Linux binaries are not provided. Users are advised to build from source. See
BUILDING.md file for instructions.

For information on using and configuring Q2PRO, refer to client and server
manuals available in doc/ subdirectory.
