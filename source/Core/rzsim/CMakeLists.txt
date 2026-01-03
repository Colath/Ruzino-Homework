# Remove the /wd5054 flag that Eigen injects into CUDA compilation
if(MSVC)
    get_target_property(_cuda_opts eigen INTERFACE_COMPILE_OPTIONS)
    list(REMOVE_ITEM _cuda_opts "/wd5054")
    set_target_properties(eigen PROPERTIES
        INTERFACE_COMPILE_OPTIONS "${_cuda_opts}"
    )
endif()

RUZINO_ADD_LIB(
	rzsim
	SHARED
	WITH_CUDA
	PUBLIC_LIBS RHI Eigen3::Eigen
	SKIP_DIRS nodes renderer_nodes geometry_nodes
)

add_subdirectory(nodes)
add_subdirectory(renderer_nodes)
add_subdirectory(geometry_nodes)
