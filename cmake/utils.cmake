# utils.cmake

#重新定义当前目标的源文件的__FILE__宏
function(redefine_file_macro targetname)
    #获取当前目标的所有源文件
    get_target_property(source_files "${targetname}" SOURCES)
    #遍历源文件
    foreach(sourcefile ${source_files})
        #获取当前源文件的编译参数
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)
        #获取当前文件的绝对路径
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        #将绝对路径中的项目路径替换成空,得到源文件相对于项目路径的相对路径
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        #将我们要加的编译参数(__FILE__定义)添加到原来的编译参数里面
        list(APPEND defs "__FILE__=\"${relpath}\"")
        #重新设置源文件的编译参数
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs}
            )
    endforeach()
endfunction()


function(ragelmaker src_rl outputlist outputdir)
    #Create a custom build step that will call ragel on the provided src_rl file.
    #The output .cpp file will be appended to the variable name passed in outputlist.

    get_filename_component(src_file ${src_rl} NAME_WE)

    set(rl_out ${outputdir}/${src_file}.rl.cc)

    #adding to the list inside a function takes special care, we cannot use list(APPEND...)
    #because the results are local scope only
    set(${outputlist} ${${outputlist}} ${rl_out} PARENT_SCOPE)

    #Warning: The " -S -M -l -C -T0  --error-format=msvc" are added to match existing window invocation
    #we might want something different for mac and linux
    add_custom_command(
        OUTPUT ${rl_out}
        COMMAND cd ${outputdir}
        COMMAND ragel ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl} -o ${rl_out} -l -C -G2  --error-format=msvc
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl}
        )
    set_source_files_properties(${rl_out} PROPERTIES GENERATED TRUE)
endfunction(ragelmaker)



# 暂时未使用
function(protobufmaker src_proto outputlist outputdir)
    #Create a custom build step that will call ragel on the provided src_rl file.
    #The output .cpp file will be appended to the variable name passed in outputlist.

    get_filename_component(src_file ${src_proto} NAME_WE)
    get_filename_component(src_path ${src_proto} PATH)

    set(protobuf_out ${outputdir}/${src_path}/${src_file}.pb.cc)

    #adding to the list inside a function takes special care, we cannot use list(APPEND...)
    #because the results are local scope only
    set(${outputlist} ${${outputlist}} ${protobuf_out} PARENT_SCOPE)

    add_custom_command(
        OUTPUT ${protobuf_out}
        COMMAND protoc --cpp_out=${outputdir} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${src_proto}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src_proto}
        )
    set_source_files_properties(${protobuf_out} PROPERTIES GENERATED TRUE)
endfunction(protobufmaker)



# 精简了cmake编译test文件
function(serverBin_add_executable targetname srcs depends libs)
    add_executable(${targetname} ${srcs})
    add_dependencies(${targetname} ${depends})
    redefine_file_macro(${targetname})
    target_link_libraries(${targetname} ${libs})
endfunction()
