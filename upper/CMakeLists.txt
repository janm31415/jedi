set(HDRS
)

set(SRCS
main.cpp
)

if (WIN32)
set(CMAKE_C_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_CXX_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_C_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi")
set(CMAKE_CXX_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi")
endif (WIN32)

# general build definitions
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_executable(upper ${HDRS} ${SRCS})

include ("../jtk/jtk/jtk.cmake")

source_group("Header Files" FILES ${HDRS})
source_group("Source Files" FILES ${SRCS})


 target_include_directories(upper
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    )	
    
target_link_libraries(upper
    PRIVATE 
    )	

