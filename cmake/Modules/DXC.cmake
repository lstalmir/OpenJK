
include(CMakeParseArguments)

unset (DXC_EXECUTABLE CACHE)

if (WIN32)
    set (DXC_EXECUTABLE "${OpenJKLibDir}/dxc/bin/dxc.exe")
    set (DXC_FOUND TRUE)

    # compiles an hlsl shader
    function (add_dxc_command)
        set (Options SPIRV)
        set (OneValueArgs TARGET STAGE ENTRY_POINT FILE OUTPUT)
        set (MultiValueArgs DEPENDS MACROS)
        cmake_parse_arguments (ARGS "${Options}" "${OneValueArgs}" "${MultiValueArgs}" ${ARGN})

        # get filename component of the input file
        get_filename_component (FILENAME ${ARGS_FILE} NAME_WE)
        set (PREPROCESSED_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}_${ARGS_STAGE}.preprocessed.hlsl")
        
        set (DXC_COMMAND "${DXC_EXECUTABLE}" ${PREPROCESSED_FILE})
        if (ARGS_SPIRV)
            list (APPEND DXC_COMMAND -spirv)
        endif (ARGS_SPIRV)
        if (ARGS_ENTRY_POINT)
            list (APPEND DXC_COMMAND -E "${ARGS_ENTRY_POINT}")
        elseif (ARGS_STAGE)
            list (APPEND DXC_COMMAND -E "${ARGS_STAGE}_Main")
        endif (ARGS_ENTRY_POINT)
        if (ARGS_STAGE)
            string (TOLOWER ${ARGS_STAGE} STAGE)
            list (APPEND DXC_COMMAND -T "${STAGE}_6_0")
        endif (ARGS_STAGE)
        if (ARGS_MACROS)
            foreach (MACRO IN LISTS ARGS_MACROS)
                list (APPEND DXC_COMMAND -D "${MACRO}")
                list (APPEND Macros "-D${MACRO}")
            endforeach (MACRO)
        endif (ARGS_MACROS)
        if (ARGS_OUTPUT)
            set (OUT "${ARGS_OUTPUT}")
        else ()
            if (ARGS_DESTINATION)
                set (OUT "${ARGS_DESTINATION}/${FILENAME}_${ARGS_STAGE}.h")
            else ()
                if (DXC_OUTPUT_DIR)
                    set (OUT "${DXC_OUTPUT_DIR}/${FILENAME}_${ARGS_STAGE}.h")
                else ()
                    set (OUT "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}_${ARGS_STAGE}.h")
                endif (DXC_OUTPUT_DIR)
            endif (ARGS_DESTINATION)
        endif (ARGS_OUTPUT)
        list (APPEND DXC_COMMAND -Fh "${OUT}")
        list (APPEND DXC_COMMAND -Vn "${FILENAME}_${ARGS_STAGE}")
        list (APPEND DXC_COMMAND -fspv-target-env=vulkan1.1)
        if (${ARGS_STAGE} EQUAL "VS")
            list (APPEND DXC_COMMAND -fvk-invert-y)
        endif ()

        add_custom_command (TARGET ${ARGS_TARGET} PRE_BUILD
            COMMAND ${CMAKE_CXX_COMPILER} -nologo -P "${ARGS_FILE}" -Fi${PREPROCESSED_FILE} ${Macros}
            COMMAND ${DXC_COMMAND}
            DEPENDS ${ARGS_DEPENDS}
            BYPRODUCTS "${OUT}" "${PREPROCESSED_FILE}")
    endfunction (add_dxc_command)
endif (WIN32)
