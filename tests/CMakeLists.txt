
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${LIBXML2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_MODEL_LIBRARY})

add_executable (db-info-test db-info-test.c)
add_executable (async-test async-test.c)
add_executable (sonar-control-model-test sonar-control-model-test.c)
add_executable (sonar-model-test sonar-model-test.c)

target_link_libraries (db-info-test ${TEST_LIBRARIES})
target_link_libraries (async-test ${TEST_LIBRARIES})
target_link_libraries (sonar-control-model-test ${TEST_LIBRARIES})
target_link_libraries (sonar-model-test ${TEST_LIBRARIES})

install (TARGETS db-info-test
                 async-test
                 sonar-control-model-test
                 sonar-model-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

