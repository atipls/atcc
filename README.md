# Ati's C-like Compiler

This is the playground for me to see what I need from a low-level language.

Created due to my interest in language development, and because I felt like C was outdated/lacking in features useful in development.

Main changes include (but not limited to):

* No headers
* Everything is 1 compilation unit
* Length-prefixed strings
* Builtin array type


C interop is not a high priority as it is currently transpiled to it

## Building

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
ninja / make
```
