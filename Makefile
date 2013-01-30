#
#   Makefile - Embedthis GoAhead Makefile wrapper for per-platform makefiles
#
#	This Makefile is for Unix/Linux and Cygwin. Use WinMake for windows.
#
#   You can use this Makefile and build via "make" with a pre-selected configuration. Alternatively,
#	you can build using the "bit" tool for for a fully configurable build. If you wish to 
#	cross-compile, you should use "bit".
#
#   Modify compiler and linker default definitions here:
#
#       export ARCH      = CPU architecture (x86, x64, ppc, ...)
#       export OS        = Operating system (linux, macosx, windows, vxworks, ...)
#       export CC        = Compiler to use 
#       export LD        = Linker to use
#       export DEBUG     = Set to debug or release for debug or optimized builds
#       export CONFIG    = Output directory for built items. Defaults to OS-ARCH-PROFILE
#       export CFLAGS    = Add compiler options. For example: -Wall
#       export DFLAGS    = Add compiler defines. For example: -DCOLOR=blue
#       export IFLAGS    = Add compiler include directories. For example: -I/extra/includes
#       export LDFLAGS   = Add linker options
#       export LIBPATHS  = Add linker library search directories. For example: -L/libraries
#       export LIBS      = Add linker libraries. For example: -lpthreads
#       export PROFILE   = Build profile, used in output products directory name
#
#	See projects/$(OS)-$(ARCH)-$(PROFILE)-bit.h for configuration default settings. Can override via
#	environment variables. For example: make BIT_PACK_SQLITE=0. These are converted to DFLAGS and 
#	will then override the bit.h default values.
#
NAME    := goahead
OS      := $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')
MAKE    := $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory
PROFILE := default
DEBUG	:= debug
EXT     := mk

ifeq ($(OS),windows)
ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
    ARCH?=x64
else
    ARCH?=x86
endif
    MAKE:= projects/windows.bat
    EXT := nmake
else
	ARCH:= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
endif
BIN 	:= $(OS)-$(ARCH)-$(PROFILE)/bin

.EXPORT_ALL_VARIABLES:

all compile:
	$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@
	@echo ; echo 'You can now install via "sudo make install" or run GoAhead via: "sudo make run"'
	@echo ; echo "To run locally, put $(OS)-$(ARCH)-$(PROFILE)/bin in your path" ; echo

clean clobber uninstall run:
	$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@

install:
	$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@
	@echo ; echo 'You can now run via "sudo goahead -v --home /etc/goahead"'

version:
	@$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@

help:
	@echo '' >&2
	@echo 'With make, the default configuration can be modified by setting make' >&2
	@echo 'variables. Set to 0 to disable and 1 to enable:' >&2
	@echo '' >&2
	@echo '  PROFILE                         # Select default or static for static linking' >&2
	@echo '  BIT_GOAHEAD_ACCESS_LOG          # Enable request access log (true|false)' >&2
	@echo '  BIT_GOAHEAD_CA_FILE             # File of client certificates (path)' >&2
	@echo '  BIT_GOAHEAD_CERTIFICATE         # Server certificate for SSL (path)' >&2
	@echo '  BIT_GOAHEAD_CIPHERS             # SSL cipher suite (string)' >&2
	@echo '  BIT_GOAHEAD_CGI                 # Enable the CGI handler (true|false)' >&2
	@echo '  BIT_GOAHEAD_CGI_BIN             # Directory CGI programs (path)' >&2
	@echo '  BIT_GOAHEAD_JAVASCRIPT          # Enable the Javascript JST handler (true|false)' >&2
	@echo '  BIT_GOAHEAD_KEY                 # Server private key for SSL (path)' >&2
	@echo '  BIT_GOAHEAD_LEGACY              # Enable the GoAhead 2.X legacy APIs (true|false)' >&2
	@echo '  BIT_GOAHEAD_LIMIT_BUFFER        # I/O Buffer size. Also chunk size.' >&2
	@echo '  BIT_GOAHEAD_LIMIT_FILENAME      # Maximum filename size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_HEADER        # Maximum HTTP single header size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_HEADERS       # Maximum HTTP header size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_NUM_HEADERS   # Maximum number of headers' >&2
	@echo '  BIT_GOAHEAD_LIMIT_PASSWORD      # Maximum password size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_PORT          # Maximum POST (and other method) incoming body size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_PUT           # Maximum PUT body size ~ 200MB' >&2
	@echo '  BIT_GOAHEAD_LIMIT_SESSION_LIFE  # Session lifespan in seconds (30 mins)' >&2
	@echo '  BIT_GOAHEAD_LIMIT_SESSION_COUNT # Maximum number of sessions to support' >&2
	@echo '  BIT_GOAHEAD_LIMIT_STRING        # Default string allocation size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_TIMEOUT       # Request inactivity timeout in seconds' >&2
	@echo '  BIT_GOAHEAD_LIMIT_URI           # Maximum URI size' >&2
	@echo '  BIT_GOAHEAD_LIMIT_UPLOAD        # Maximum upload size ~ 200MB' >&2
	@echo '  BIT_GOAHEAD_LISTEN              # Addresses to listen to (["http://IP:port", ...])' >&2
	@echo '  BIT_GOAHEAD_LOGFILE             # Default location and level for debug log (path:level)' >&2
	@echo '  BIT_GOAHEAD_LOGGING             # Enable application logging (true|false)' >&2
	@echo '  BIT_GOAHEAD_PAM                 # Enable Unix Pluggable Auth Module (true|false)' >&2
	@echo '  BIT_GOAHEAD_PUT_DIR             # Define the directory for file uploaded via HTTP PUT (path)' >&2
	@echo '  BIT_GOAHEAD_REALM               # Authentication realm (string)' >&2
	@echo '  BIT_GOAHEAD_REPLACE_MALLOC      # Replace malloc with non-fragmenting allocator (true|false)' >&2
	@echo '  BIT_GOAHEAD_SSL                 # To enable SSL' >&2
	@echo '  BIT_GOAHEAD_STATIC              # Build with static linking (true|false)' >&2
	@echo '  BIT_GOAHEAD_STEALTH             # Run in stealth mode. Disable OPTIONS, TRACE (true|false)' >&2
	@echo '  BIT_GOAHEAD_TRACING             # Enable debug tracing (true|false)' >&2
	@echo '  BIT_GOAHEAD_TUNE                # Optimize (size|speed|balanced)' >&2
	@echo '  BIT_GOAHEAD_UPLOAD              # Enable file upload (true|false)' >&2
	@echo '  BIT_GOAHEAD_UPLOAD_DIR          # Define directory for uploaded files (path)' >&2
	@echo '  BIT_PACK_EST                    # Enable the EST SSL stack' >&2
	@echo '  BIT_PACK_MOCANA                 # Enable the Mocana NanoSSL stack' >&2
	@echo '  BIT_PACK_MATRIXSSL              # Enable the MatrixSSL SSL stack' >&2
	@echo '  BIT_PACK_OPENSSL                # Enable the OpenSSL SSL stack' >&2
	@echo '  BIT_ROM                         # Build for ROM without a file system' >&2
	@echo '' >&2
	@echo 'For example, to disable CGI:' >&2
	@echo '' >&2
	@echo '      make BIT_GOAHEAD_CGI=0' >&2
	@echo '' >&2
	@echo 'Other make variables include:' >&2
	@echo '' >&2
	@echo '      ARCH, CC, CFLAGS, DFLAGS, IFLAGS, LD, LDFLAGS, LIBPATHS, LIBS, OS' >&2
	@echo '' >&2
	@echo 'Alternatively, for faster, easier and fully configurable building, install' >&2
	@echo 'bit from http://embedthis.com/downloads/bit/download.ejs and re-run'>&2
	@echo 'configure and then build with bit.' >&2
	@echo '' >&2
