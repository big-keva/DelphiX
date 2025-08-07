if (SANITIZER_ENABLED)
	add_compile_options(-fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=leak)
	add_link_options(-fsanitize=address)
endif()

link_libraries(DelphiX mtc moonycode)

add_executable(test-DelphiX-enquote
	enquote/test-compressor.cpp
	test-main.cpp)

add_executable(test-DelphiX-context
	context/test-key.cpp
	context/test-contents.cpp
	context/test-processor.cpp
	context/test-fields-manager.cpp
	context/test-mini-queries.cpp
	test-main.cpp)

add_executable(test-DelphiX-queries
	queries/test-queries-parser.cpp
	test-main.cpp)

add_executable(test-DelphiX-textAPI
	textAPI/test-textAPI.cpp
	test-main.cpp)

add_executable(test-DelphiX-storage
	storage/test-storage-fs-based.cpp
	test-main.cpp)

add_executable(test-DelphiX-indexer
	indexer/test-commit-contents.cpp
	indexer/test-dynamic-chains.cpp
	indexer/test-dynamic-chains-ringbuffer.cpp
	indexer/test-dynamic-contents.cpp
	indexer/test-dynamic-entities.cpp
	indexer/test-index-layers.cpp
	indexer/test-layered-contents.cpp
	indexer/test-patch-table.cpp
	indexer/test-static-contents.cpp
	indexer/test-static-entities.cpp
	indexer/test-stream-indexing.cpp
	test-main.cpp)
