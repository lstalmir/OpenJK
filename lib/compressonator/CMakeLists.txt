
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(arch x86)
else()
    set(arch x64)
endif()

# Let consumers get at our bundled copy in the standard CMake way.  These
# variables are not set in the cache, but instead shadow the variables in the
# cache.
set(compressonator_LIBRARIES "${CMAKE_SOURCE_DIR}/lib/compressonator/lib/${arch}/CMP_Core.lib" PARENT_SCOPE)
set(compressonator_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/lib/compressonator/include/" PARENT_SCOPE)

mark_as_advanced(compressonator_LIBRARIES compressonator_INCLUDE_DIRS)

# Create an interface target for easier linking
add_library(compressonator INTERFACE)
target_include_directories(compressonator INTERFACE ${compressonator_INCLUDE_DIRS})
target_link_libraries(compressonator INTERFACE ${compressonator_LIBRARIES})
