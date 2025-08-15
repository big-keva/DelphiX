add_executable(DX-merge
	merge-indices.cpp)

add_executable(DX-check
	index-check.cpp)

target_link_libraries(DX-merge
	DelphiX
	mtc)

target_link_libraries(DX-check
	DelphiX
	mtc)
