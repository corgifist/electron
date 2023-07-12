g++ -c impl/render_preview_impl.cpp -I src/ -g -fPIC -m64 -DELECTRON_IMPLEMENTATION_MODE -std=c++20
g++ -shared -o librender_preview_impl.so render_preview_impl.o  -lstdc++ -dl -fPIC -g -L. -m64
rm render_preview_impl.o
