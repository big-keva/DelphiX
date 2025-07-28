add_compile_definitions(
	STEMKA_SO_PATH=${stemka_api_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}stemka-api${CMAKE_SHARED_LIBRARY_SUFFIX})

add_executable(test-DelphiX-stemka-api
	test-lang-stemka.cpp
	../../../tests/test-main.cpp)

add_dependencies(test-DelphiX-stemka-api stemka-api)

target_link_libraries(test-DelphiX-stemka-api DelphiX moonycode mtc)