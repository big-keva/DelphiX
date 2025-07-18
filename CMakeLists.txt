cmake_minimum_required(VERSION 3.1)
project(DelphiX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

include_directories(
	./
	third-party)

add_compile_options(-mfpmath=sse -fstack-protector-all -g
	-W -Wall -Wextra -Wcast-align -pedantic -Wfloat-equal -Wpointer-arith -Wformat-security
	-Wmissing-format-attribute -Wformat=1 -Wwrite-strings -Wcast-align -Wno-long-long
	-Woverloaded-virtual -Wno-suggest-attribute=format)

add_subdirectory(third-party/moonycode)
add_subdirectory(third-party/mtc)

add_library(DelphiX
	src/contents.cpp
	src/primes.cpp

	src/context/decomposer.cpp
	src/context/index-keys.cpp
	src/context/make-image.cpp

	src/indexer/commit-contents.cpp
	src/indexer/contents-index-merger.cpp
	src/indexer/dynamic-contents.cpp
	src/indexer/index-layers.cpp
	src/indexer/layered-contents.cpp
	src/indexer/merger-contents.cpp
	src/indexer/override-entities.cpp
	src/indexer/static-contents.cpp

	src/storage/posix-fs-output.cpp
	src/storage/posix-fs-serial.cpp
	src/storage/posix-fs-storage.cpp
	src/storage/posix-fs-policies.cpp

	src/textAPI/dump-as-json.cpp
	src/textAPI/dump-as-tags.cpp
	src/textAPI/load-as-json.cpp
	src/textAPI/load-as-tags.cpp)

add_subdirectory(tests)
