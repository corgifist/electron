g++ -c impl/rect2d_layer.cpp -I src/ -g -fPIC -m64 -DELECTRON_IMPLEMENTATION_MODE -std=c++20
g++ -shared -o librect2d_layer.so rect2d_layer.o  -lstdc++ -ldl -fPIC -g -L. -m64
rm rect2d_layer.o
