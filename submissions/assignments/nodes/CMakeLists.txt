set(deps
		stage nodes_system usd geometry usdShade Eigen3::Eigen autodiff igl::core igl_restricted::triangle
)
message(STATUS "[Utils status: ${student_name}_${util_lib_target_name}_AVAILABLE] " ${${student_name}_${util_lib_target_name}_AVAILABLE})
if (${student_name}_${util_lib_target_name}_AVAILABLE)
	message(STATUS "[${student_name}_${util_lib_target_name}_AVAILABLE] " Available!)
	list(APPEND deps ${student_name}_${util_lib_target_name})
endif ()

add_nodes(
	TARGET_NAME ${student_name}_nodes_hw_submissions
	DEP_LIBS ${deps}
	COMPILE_DEFS NOMINMAX
)
