
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    hyscanmodel)

add_executable (db-info-test db-info-test.c)
target_link_libraries (db-info-test ${TEST_LIBRARIES})

add_executable (async-test async-test.c)
target_link_libraries (async-test ${TEST_LIBRARIES})
