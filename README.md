Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly) request that to
preserve their value as a teaching tool, solutions not be posted
publicly by anybody.

Website: https://cs144.stanford.edu

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`

**测试截图**

check0：

![image-20230627161959748](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627161959748.png)

check1：

![image-20230627162018039](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627162018039.png)

check2：

![image-20230627162036239](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627162036239.png)

check3：

![image-20230627162054890](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627162054890.png)

check4：

![image-20230627162108448](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627162108448.png)

check5：

![image-20230627162118290](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627162118290.png)

lab6：发送文字等测试通过，但由于客观网络原因发送整个文件会失败，只会有一部分（应该是被墙中断，发送一小段文字就会超级慢）

![image-20230627162344871](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20230627162344871.png)
