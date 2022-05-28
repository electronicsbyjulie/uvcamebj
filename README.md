This is the source code for the software for the Electronics by Julie UV Camera.

After downloading the files to the home folder on your Pi, please run the following commands in a terminal:

<code>sudo apt-get install -y realvnc-vnc-server</code><br>
<code>sudo apt-get install -y gedit</code><br>
<code>sudo apt-get install -y rclone</code><br>
<code>sudo apt-get install -y unclutter</code><br>
<code>sudo apt-get install -y libgtk-3-dev</code><br>
<code>sudo apt-get install -y libi2c-dev</code><br>
<code>sudo apt-get install -y libjpeg-dev</code><br>
<code>sudo apt-get install -y libpng-dev</code><br>
<code>cd ~</code><br>
<code>./uvcaminstall.sh</code><br>

<h3>Explanation of the Color Modes</h3>

Since the human eye does not see ultraviolet as a separate color, not even when it is allowed to reach the retina, and the screen cannot display ultraviolet, it is necessary to remap the color channels to RGB space.

Logically, it would make sense to route the image's UV data to the red channel, and the raw mode indeed does something very much like this. The raw mode saves the pixels from the Pi camera image, with no change to the colors. This however captures a world of almost exclusively greens, blue-greens, and magenta.

To address the aesthetics of a green and purple world, we created the CUV (color ultraviolet) mode. Similar to CIR film and our infrared camera's CIR mode, this mode remaps the channels so that the longest wavelengths appear as red and the shortest as blue. Because the camera interprets ultraviolet as magenta, the UV channel is partially subtracted from the blue channel before remapping. As a result, things like sky, bark, rock, and skin look natural since their CUV colors are close to their RGB colors, and UV-absorbing white objects appear yellow.

If you're only interested in the UV channel, whether for artistic effect or for a scientific usage, we also created a monochrome mode that isolates only the ultraviolet from the image and discards all other wavelengths.

We also added another mode we call bee mode. It's true that no one can know what anything actually looks like to an insect (look up qualia if you're interested - also insects use a different part of their brain for vision than we use, so they probably don't experience sight the same way at all!) but we like to imagine what the closest equivalent might be. Bee mode interprets green as yellow, blue as aqua, and UV as magenta. This produces a fairly natural appearance for the greens of foliage and renders most flowers in the aqua-blue-purple-pink range. UV-absorbing white renders as mint green, in parallel to bee vision which has kelly green as complementary to UV. The only strange color involved is a purple sky and purple sheen on the tops of leaves.
