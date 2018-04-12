## Violet

A set of **C++17** utilities (echo) I make in my spare time all wrapped up in an HTTP server (with *OpenSSL* support). All licensed under GPL v3.  

*Requires clang 5 or gcc 7 for C++17 support.*  

As of now, violet uses *lodepng* for captcha image encoding.  

Build using:
```bash
$ mkdir .build
$ cd .build && cmake [-DOPENSSL=FALSE] ../src && make
```