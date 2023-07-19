g++ -c impl/sdf2d_layer.cpp -I src/ -g -fPIC -m64 -DELECTRON_IMPLEMENTATION_MODE -std=c++20
g++ -shared -o libsdf2d_layer.so sdf2d_layer.o  -lstdc++ -ldl -fPIC -g -L. -m64
rm sdf2d_layer.o
