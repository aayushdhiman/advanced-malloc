# Advanced Memory Allocator

A more advanced memory allocator that doesn't used deprecated functionality and uses threads to speed up mimicked malloc, calloc, and free. Writtenn for a systems class.

Completed 11/30/2022.

The [Makefile](Makefile) contains the following targets:

- `make all` - compile [mymalloc.c](mymalloc.c) into the object file `mymalloc.o`
- `make test` - compile and run tests in the [tests](tests/) directory with `mymalloc.o`.
- `make demo` - compile and run tests in the tests directory with standard malloc.
- `make clean` - perform a minimal clean-up of the source tree
- `make help` - print available targets

