cmake_minimum_required(VERSION 3.0)

IF(NOT APPLE)
	MESSAGE(FATAL_ERROR "External only for OSX.")
ENDIF()

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-api/script/max-pretarget.cmake)

include_directories( 
	"${C74_INCLUDES}"
)


add_library( 
	${PROJECT_NAME} 
	MODULE
	${PROJECT_NAME}.cpp
)

find_library(LIBUSB 
	libusb-1.0.a
	HINTS .
)

target_link_libraries(${PROJECT_NAME} 
	PUBLIC 
	${LIBUSB} 
	"-framework CoreFoundation" 
	"-framework IOKit"
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-api/script/max-posttarget.cmake)
