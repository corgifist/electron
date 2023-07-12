pkill Electron
rm Electron
./generate_headers.sh
cmake . -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
make
python gen_batch.py project_configuration_impl
./compile_impl.sh
python gen_batch.py render_preview_impl
./compile_impl.sh
./Electron