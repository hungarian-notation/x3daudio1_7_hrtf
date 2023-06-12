# X3DAudio HRTF
## HRTF for Arma 3, Skyrim, Fallout 4 and potentially other titles using X3dAudio + XAudio2 (including ones running on Unreal Engine 4 and UDK).

[![Youtube video](https://s15.postimg.org/slkiotxp7/skyrim.png)](https://www.youtube.com/watch?v=jJPpqssMPl0)
[![Youtube video](http://s24.postimg.org/iozt6p9ut/skyrim_fake_youtube.png)](http://www.youtube.com/watch?v=hsCgaMeTVG0)

### Tags
HRTF, HRIR, Binaural sound, Spatial sound, Ambisonics

### Download

Visit [releases page](https://github.com/kosumosu/x3daudio1_7_hrtf/releases) and grab the latest one. Ensure the version you download matches version of the game (x86 or x64).

### Installation
#### For Unreal Engine 4 based games
First, you must locate the _true_ main executable. The one in the root folder is not the one. Inside the root folder there must be another folder that usually has the same name as the game. Within it there must be a folder Binaries\Win64 or Win32. Examples:
* **Squad**: SteamLibrary\steamapps\common\Squad\Squad\Binaries\Win64
* **The Vanishing of Ethan Carter Redux**: SteamLibrary\steamapps\common\The Vanishing of Ethan Carter Redux\EthanCarter\Binaries\Win64

Then unpack the archive into this folder, so it's contents lie next to the *true* main executable. Ensure you use the correct (x64 or x86) version of the x3daudio_hrtf.
#### For UDK based games
Same as for UE4, but Binaries folder lies right in the root folder. Examples:
* **The Vanishing of Ethan Carter**: SteamLibrary\steamapps\common\The Vanishing of Ethan Carter\Binaries\Win64

#### For other games
Unpack the contents of the package to the game folder where the main executable is located. Ensure it is the exe of the game itself, not a launcher. Enjoy!

Note: some anti-cheat software (e.g. BattleEye) won't let you hook anything into the game, so you will have to disable it. That means no HRTF in multiplayer.

#### Notes for developers
##### How it works
Game hooks both x3daudio1_x.dll, which is simple, and IXAudio2 COM interface, which is a less trivial thing to do.
X3daudio proxy remembers 3D audio position into repository. To mark sound as 3D it sets the first coefficient of the mixing matrix to NaN. And the second one is set to sound ID, so we can later identify it. Let's hope game is not that stupid and won't pass a 1x1 matrix.
Then, when game calls IXAuido2Voice::SetOutputMatrix(), we check if 1st coeff == NaN which means we have to enable our XAPO DSP effect and pass parameters to it (position, etc).

##### Arma 3
Works well now. No tricky things there. Has pretty complex audio mixing graph, but nothing wierd. BattleEye must be disabled in order to work.

##### Arma 2
Has glitches. I haven't investigated them yet. Most sounds were fine, but HMMWV's engine sound was coming from a completely different direction when I was playing. BattleEye must be disabled in order to work.

##### TES V Skyrim (including Special Edition)
Works perfectly now.
Skyrim always creates Mastering voice as six-channel. We override it to two-channel.
For Special Edition use x64 version.

##### Fallout 4
Works well. Uses the same codebase as Skyrim, so nothing different here. The only difference is x64 platform.

##### Unreal Engine 4 and UDK based games
Works well. UE does not pass distance to X3DAudio, only direction. The distance between listener and emitter is always 1. UE calculates volume by itself, so the volume is passed in the matrix.

### Legal Stuff
#### X3DAudio HRTF
The given library is licensed under the GPLv3.

#### [Mhook](http://codefromthe70s.org/mhook24.aspx)
Copyright (c) 2007-2012, Marton Anka
Portions Copyright (c) 2007, Matt Conover

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
