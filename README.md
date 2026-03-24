# ZoeVM

Just a simple stack-based virtual machine.

Years ago, I made a shitty VM in C called `zoe`, and I recently found its
source code and felt nostalgic. I decided to rewrite it since I am currently
unemployed.

The VM itself is implemented as a single header library `sauce/zoe.h`.
It does not implement any builtin functions, and those must be implemented and
added to the VM by whoever initializes it ([see `sauce/main.c`](./sauce/main.c)).

This repository also includes an example compiler for a weird stack-based language
written in my other old toy scripting language ([see nero](https://github.com/pra1rie/nerolang)).

