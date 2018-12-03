Embedthis GoAhead
===

[![Build Status](https://travis-ci.org/embedthis/goahead.png)](https://travis-ci.org/embedthis/goahead)

The most popular little embedded web server.

Branches
---
The repository has several branches:

* master - Most recent release of the software.
* dev - Current ongoing development.
* X.X - Archived prior release branches for maintenance.

Licensing
---
See [LICENSE.md](LICENSE.md) and https://www.embedthis.com/goahead/licensing.html for details.

### Documentation

  See https://www.embedthis.com/goahead/doc/index.html.

### Building from Source

You can build GoAhead with make, Visual Studio, Xcode or [MakeMe](https://www.embedthis.com/makeme/).

The IDE projects and Makefiles will build with [ESP](https://www.embedthis.com/esp/) and SSL using the [MbedTLS](https://github.com/ARMmbed/mbedtls) TLS stack. To build with CGI, OpenSSL or other modules, read the [projects/README.md](projects/README.md) for details.

### To Build with Make

#### Linux or MacOS

    make

or to see the commands as they are invoked:

    make SHOW=1

You can pass make variables to tailor the build. For a list of variables:

	make help

To run

	make run

#### Windows

First open a Windows cmd prompt window and then set your Visual Studio environment variables by running vcvarsall.bat from your Visual Studio installation folder.

Then run a Windows cmd prompt window and type:

    make

### To Build with Visual Studio

Open the solution file at:

    projects/goahead-windows-default.sln

Then select Build -> Solution.

To run the debugger, right-click on the "goahead" project and set it as the startup project. Then modify the project properties and set the Debugging configuration properties. Set the working directory to be:

    $(ProjectDir)\..\..\test

Set the arguments to be
    -v

Then start debugging.

### To Build with Xcode.

Open the solution file:

    projects/goahead-macosx-default.sln

Choose Product -> Scheme -> Edit Scheme, and select "Build" on the left of the dialog. Click the "+" symbol at the bottom in the center and then select all targets to be built. Before leaving this dialog, set the debugger options by selecting "Run/Debug" on the left hand side. Under "Info" set the Executable to be "goahead", set the launch arguments to be "-v" and set the working directory to be an absolute path to the "./test" directory in the goahead source. The click "Close" to save.

Click Project -> Build to build.

Click Project -> Run to run.

### To build with MakeMe

To install MakeMe, download it from https://www.embedthis.com/makeme/.

    ./configure
    me

For a list of configure options:

	./configure --help

### To run

    me run

### To install

If you have built from source using Make or MakeMe, you can install the software using:


    sudo make install

or

    sudo me install

### To uninstall

    sudo make uninstall

or

    sudo me uninstall

### To test

    me test

Resources
---
  - [GoAhead web site](https://www.embedthis.com/goahead/)
  - [Embedthis web site](https://www.embedthis.com/)
  - [GoAhead GitHub repository](http://github.com/embedthis/goahead)
