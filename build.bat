cl /O2 /Zi /Fe:pbf.exe /I third_party\SDL2\include main.cpp /link /subsystem:console /libpath:third_party\SDL2\lib_x64 Shell32.lib Opengl32.lib SDL2.lib SDL2main.lib
