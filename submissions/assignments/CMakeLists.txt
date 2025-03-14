set(student_name "assignments") # 你需要在你自己的文件夹下修改这里！
set(util_lib_target_name "util_lib")
set(${student_name}_${util_lib_target_name}_AVAILABLE FALSE)
add_subdirectory(utils)
add_subdirectory(nodes)
