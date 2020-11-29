## OpenJDK for iOS

 - The source is forked from http://hg.openjdk.java.net/mobile/jdk9/ and modified to compile on macOS 10.15+
 - It relies on [Zero-Assembler Project](https://openjdk.java.net/projects/zero/), a portable implementation of OpenJDK
## Building
- Xcode command-line tools and iPhoneOS SDKs (12.0+) are required
- For building on Linux Darwin [cctools port](https://github.com/tpoechtrager/cctools-port) is required. 
- freetype, libffi, and CUPS are also required (available in release)
- configure-note file has the comprehensive instructions for building
