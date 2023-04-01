#include "IOCoroutineScheduler/uri.h"
#include <iostream>

int main(int argc, char** argv){
    //bin::Uri::ptr uri = bin::Uri::Create("http://www.bin.top/test/uri?id=100&name=bin#frg");
    bin::Uri::ptr uri = bin::Uri::Create("http://admin@www.bin.top/test/中文/uri?id=100&name=bin&vv=中文#frg中文");
    //bin::Uri::ptr uri = bin::Uri::Create("http://admin@www.bin.top");
    //bin::Uri::ptr uri = bin::Uri::Create("http://www.bin.top/test/uri");
    std::cout << uri->toString() << std::endl;
    auto addr = uri->createAddress();
    std::cout << *addr << std::endl;
    return 0;
}
