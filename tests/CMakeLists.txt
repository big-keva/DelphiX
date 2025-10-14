if(FALSE)
	add_library(test-DelphiX-helpers
			toolbox/tmppath.cpp
			toolbox/dirtool.cpp)

	link_libraries(DelphiX test-DelphiX-helpers mtc moonycode)

	add_executable(test-DelphiX-enquote
		enquote/test-quote.cpp
		test-main.cpp)

	add_executable(test-DelphiX-context
		context/test-key.cpp
		context/test-contents.cpp
		context/test-processor.cpp
		context/test-tag-compressor.cpp
		context/test-fields-manager.cpp
		test-main.cpp)

	add_executable(test-DelphiX-queries
		queries/test-queries-parser.cpp
		queries/test-rich-queries.cpp
		queries/test-mini-queries.cpp
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
else()
	link_libraries(DelphiX mtc moonycode)

	add_executable(test-DelphiX
			test-main.cpp

			toolbox/tmppath.cpp
			toolbox/dirtool.cpp

			enquote/test-quote.cpp

			context/test-key.cpp
			context/test-contents.cpp
			context/test-processor.cpp
			context/test-tag-compressor.cpp
			context/test-fields-manager.cpp

			queries/test-queries-parser.cpp
			queries/test-rich-queries.cpp
			queries/test-mini-queries.cpp

			textAPI/test-textAPI.cpp

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
			indexer/test-stream-indexing.cpp)

endif()
