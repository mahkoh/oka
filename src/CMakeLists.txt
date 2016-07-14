subdirs(plugins utils)

file(GLOB SOURCES "*.c")
add_executable(oka ${SOURCES})
target_link_libraries(oka utils pthread rt dl)
install(TARGETS oka DESTINATION lib/oka)
