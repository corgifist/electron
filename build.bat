@echo off
taskkill /IM Electron.exe
rm Electron.exe
python generate_ui_core_header.py src/ui_core.h
python generate_ui_api.py src/ui_api.h
cmake .
make
python gen_batch.py project_configuration_impl
call compile_impl.bat
python gen_batch.py render_preview_impl
call compile_impl.bat
Electron.exe