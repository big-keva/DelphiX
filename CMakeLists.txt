cmake_minimum_required(VERSION 3.1)
project(DelphiX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-mfpmath=sse -fstack-protector-all -g
	-W -Wall -Wextra -Wcast-align -pedantic -Wfloat-equal -Wpointer-arith -Wformat-security
	-Wmissing-format-attribute -Wformat=1 -Wwrite-strings -Wcast-align -Wno-long-long
	-Woverloaded-virtual -Wno-suggest-attribute=format)

if (SANITIZER_ENABLED)
	add_compile_options(-fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak)
	add_link_options(-fsanitize=address)
endif()

if (PROFILER_ENABLED)
	add_compile_options(-p -pg)
	add_link_options(-p -pg)
endif()

include_directories(
	./
	contrib)

message(STATUS "Include directories: " ${INCLUDE_DIRECTORIES})

if (NOT TARGET moonycode)
	add_subdirectory(contrib/moonycode)
endif()

if (NOT TARGET mtc)
	add_subdirectory(contrib/mtc)
endif()

add_library(DelphiX
	src/contents.cpp
	src/primes.cpp

	src/context/lemmatizer.cpp
	src/context/index-keys.cpp
	src/context/x-contents.cpp
	src/context/fields-man.cpp
	src/context/processor.cpp
	src/context/formats.cpp

	src/enquote/compressor.cpp

	src/indexer/commit-contents.cpp
	src/indexer/contents-index-merger.cpp
	src/indexer/dynamic-contents.cpp
	src/indexer/index-layers.cpp
	src/indexer/layered-contents.cpp
	src/indexer/merger-contents.cpp
	src/indexer/override-entities.cpp
	src/indexer/static-contents.cpp
	src/indexer/strmatch.cpp

	src/queries/parser.cpp
	src/queries/query-terms.cpp
	src/queries/query-tools.cpp
	src/queries/base-queries.cpp
	src/queries/rich-queries.cpp
	src/queries/mini-queries.cpp

	src/storage/posix-fs-output.cpp
	src/storage/posix-fs-serial.cpp
	src/storage/posix-fs-storage.cpp
	src/storage/posix-fs-policies.cpp
	src/storage/posix-fs-dump-store.cpp

	src/textAPI/dump-as-json.cpp
	src/textAPI/dump-as-tags.cpp
	src/textAPI/load-as-json.cpp
	src/textAPI/load-as-tags.cpp
	src/textAPI/text-toolkit.cpp)

# target_include_directories(DelphiX PUBLIC
#		${CMAKE_CURRENT_SOURCE_DIR}
#		${CMAKE_CURRENT_SOURCE_DIR}/contrib)

add_subdirectory(addon)
add_subdirectory(tests)
add_subdirectory(utils)
