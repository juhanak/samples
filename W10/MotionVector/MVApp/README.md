# MotionVector

This sample app shows how to use FFmpeg library to extract motion vectors from the mpeg video. 

Project contains code from FFmpegInterop library for Windows. Original code is available here: https://github.com/Microsoft/FFmpegInterop

##Prerequisites
Getting a compatible build of FFmpeg is required for this to work. FFmpeg library is included into the project as a submodule so taking recursive clone of the project will also clone ffmpeg library. Compiling FFmpeg for Windows is covered here in details: https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT

Expectation is that ffmpeg libraries and headers locate next to MVApp folder. This should automatically happen if the project is recursively cloned. 
The project is tested with "Windows 10 x86" builds. And it has dependency to FFmpeg dlls that locate "..\..\ffmpeg\Build\Windows10\x86\bin" relative to the project. If x64 libraries are built then then one needs to modify project files a little bit that required headers and libs are found. 

More information on the project available on this page:
http://juhana.cloudapp.net/?p=300
