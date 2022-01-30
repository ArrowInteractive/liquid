rm build
mkdir build

cd build
cmake -G "Ninja" ..
make

cd .. 