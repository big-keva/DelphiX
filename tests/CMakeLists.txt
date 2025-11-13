set(COMMON_SRC toolbox/tmppath.cpp toolbox/dirtool.cpp test-main.cpp)

link_libraries(DelphiX DeliriX mtc moonycode)

if(FALSE)
	add_executable(test-DelphiX-enquote
		enquote/test-quote.cpp
		${COMMON_SRC})

	add_executable(test-DelphiX-context
		context/test-key.cpp
		context/test-contents.cpp
		context/test-processor.cpp
		context/test-tag-compressor.cpp
		context/test-fields-manager.cpp
		${COMMON_SRC})

	add_executable(test-DelphiX-queries
		queries/test-queries-parser.cpp
		queries/test-rich-queries.cpp
		queries/test-mini-queries.cpp
		${COMMON_SRC})

	add_executable(test-DelphiX-storage
		storage/test-storage-fs-based.cpp
		${COMMON_SRC})

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
		${COMMON_SRC})

else()
	add_executable(test-DelphiX
		enquote/test-quote.cpp

		context/test-key.cpp
		context/test-contents.cpp
		context/test-processor.cpp
		context/test-tag-compressor.cpp
		context/test-fields-manager.cpp

		queries/test-queries-parser.cpp
		queries/test-rich-queries.cpp
		queries/test-mini-queries.cpp

		storage/test-storage-fs-based.cpp

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
		${COMMON_SRC})

endif()
