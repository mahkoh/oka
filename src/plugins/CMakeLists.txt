file(GLOB PULSE_SOURCES "pulse/*.c")
add_library(pulse MODULE ${PULSE_SOURCES})
target_link_libraries(pulse utils -lpulse)
install(TARGETS pulse DESTINATION lib/oka/plugins)

add_library(mpg123 MODULE mpg123.c)
target_link_libraries(mpg123 utils -lmpg123)
install(TARGETS mpg123 DESTINATION lib/oka/plugins)
