gen_batch() {
    python gen_batch.py $1
    ./compile_impl.sh 
}

compile_layer() {
    gen_batch $1
    mv lib$1.so layers/lib$1.so
}

pkill Electron
rm Electron
./generate_headers.sh
cmake . -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
make

gen_batch project_configuration_impl
gen_batch render_preview_impl

compile_layer rect2d_layer

./Electron