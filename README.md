#git clone from https://github.com/dusty-nv/jetson-inference.git#
1.remove deepnet test 
2.gstreamer decode multirtsp from HK ipcamera
#build#
mkdir build
cd build
cmake ..
make -j4

