add_nodes(
	TARGET_NAME rzsim_geometry_nodes
	JSON_DIR Plugins
	DEP_LIBS geometry GPUContext rzsim
	EXTRA_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
