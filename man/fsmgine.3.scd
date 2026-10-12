FSMgine(3)

# NAME

FSMgine - finite state machine engine (library)

# SYNOPSIS

Use the CMake package from C++:

	fetch or find_package(FSMgine CONFIG REQUIRED)
	target_link_libraries(app PRIVATE FSMgine::FSMgine)

# DESCRIPTION

FSMgine creates finite state machines via code generation. The main header is
_<FSMgine/FSMgine.hpp>_; see _FSMBuilder.hpp_ for building machines and
_compiled/Machine.hpp_ for the generated form. A multi-threaded variant,
FSMgineMT, links the same way (_FSMgine::FSMgineMT_).

Two CMake packages are installed: *FSMgine* and *FSMgineMT*, each with
version and target files under _lib/cmake/_.

# FILES

_/usr/include/FSMgine/_
	Library headers.

_/usr/lib/libFSMgine.so.2.2.0_
	Shared libraries (runtime in libfsmgine1, development files in
	libfsmgine-dev).

_/usr/lib/cmake/FSMgine/, /usr/lib/cmake/FSMgineMT/_
	CMake package configs.

# SEE ALSO

https://github.com/HarryPehkonen/FSMgine - full documentation and examples

# AUTHORS

Harri Pehkonen <harry.pehkonen@gmail.com>
