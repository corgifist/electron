@echo off
g++ -c impl/render_preview_impl.cpp -I src/ -g -fPIC -m64 -DELECTRON_IMPLEMENTATION_MODE
g++ -shared -o render_preview_impl.dll render_preview_impl.o -lglfw3 -lopengl32 -fPIC -g -mwindows -L. -m64
