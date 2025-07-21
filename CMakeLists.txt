cmake_minimum_required(VERSION 3.10)

project(gravitationalMass)

# Create the executable with source files
add_executable(gravitationalMass ${CMAKE_SOURCE_DIR}/src/Main.c ${CMAKE_SOURCE_DIR}/src/circularBuffer.c ${CMAKE_SOURCE_DIR}/src/objects.c ${CMAKE_SOURCE_DIR}/src/textLabel.c)


# Include directories for SDL3
target_include_directories(gravitationalMass PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_directories(gravitationalMass PUBLIC
	${CMAKE_SOURCE_DIR}/lib
)


# Link libraries for SDL3 and SDL3_ttf
target_link_libraries(gravitationalMass PUBLIC 
	SDL3
	SDL3_ttf
	m
)

