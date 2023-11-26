add_library(spirv-reflect STATIC 
            spirv_reflect.h
            spirv_reflect.c)

target_include_directories(spirv-reflect PUBLIC .)