
add_executable (db-info-test db-info-test.c)

target_link_libraries (db-info-test ${GLIB2_LIBRARIES} ${LIBXML2_LIBRARIES} ${HYSCAN_LIBRARIES} hyscanmodel)
