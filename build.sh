gen_batch() {
    python gen_batch.py $1
    ./compile_impl.sh 
}

compile_layer() {
    gen_batch $1
    if [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
        echo Moving 
        mv lib$1.so layers/lib$1.so
    else
        echo Moving $1.dll
        mv $1.dll layers/$1.dll
    fi
}

pkill Electron
rm Electron.exe
rm *.so
rm *.dll
rm -r layers
mkdir layers
./generate_headers.sh
cmake . -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
make

gen_batch project_configuration_impl
gen_batch render_preview_impl

compile_layer sdf2d_layer

./Electron