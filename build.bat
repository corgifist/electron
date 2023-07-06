@echo off
taskkill /IM Electron.exe
rm Electron.exe
call generate_headers.bat
cmake . -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++
make
python gen_batch.py project_configuration_impl
call compile_impl.bat
python gen_batch.py render_preview_impl
call compile_impl.bat
Electron.exe