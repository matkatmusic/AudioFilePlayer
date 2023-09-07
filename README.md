# AudioFilePlayer
A port of the juce AudioFilePlayerDemo into Plugin format

When you clone this repository, be sure to enable "RECURSE SUBMODULES" in whatever tool you're using the clone, so that the correct version of juce that this plugin is built with is also cloned.

This project contains a submodule of the JUCE framework.  This submodule set up to automatically check out the v7.0.5 version of JUCE.

I recommend building the version of Projucer that is within this submodule (JUCE/Extras/Projucer/Builds/), and using that build of projucer to open the AudioFilePlayer.jucer file.

If you do this, everything should "just work" when you: 
- open the AudioFilePlayer.jucer file, 
- save/open in IDE, and then 
- compile it for your operating system.

you should experience zero errors if you use the submodule's projucer build to generate the SLN/XCodeProj files.
