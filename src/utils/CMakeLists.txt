file(GLOB SOURCES "*.c")
add_library(utils SHARED ${SOURCES})
install(TARGETS utils DESTINATION lib/oka)
