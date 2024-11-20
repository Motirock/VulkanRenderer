cmake_minimum_required(VERSION 3.10)
project(MainProgram)

#Set compiler flags
set(CMAKE_CXX_FLAGS "-std=c++17 -O3 -I${CMAKE_SOURCE_DIR}/include")

#Set linker flags
set(LDFLAGS "-lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi")

#Add source files
file(GLOB SOURCES "src/*.cxx")

#Create output directory for the executable
set(EXECUTABLE_PATH "${CMAKE_BINARY_DIR}/bin/main")

#Add the target for compiling and linking
add_executable(main ${SOURCES})

#Specify the linker flags
target_link_libraries(main ${LDFLAGS})

#Set the output directory for the executable
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

#Custom target for cleaning
add_custom_target(clean_executable
    COMMAND rm -f ${EXECUTABLE_PATH}
    COMMENT "Cleaning executable"
)

#Custom target to execute the program after building
add_custom_target(run
    COMMAND "./bin/main"
    DEPENDS main
    COMMENT "Running the program"
)