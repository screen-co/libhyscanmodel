
set (TEST_LIBRARIES
     ${GLIB2_LIBRARIES}
     ${LIBXML2_LIBRARIES}
     ${HYSCAN_LIBRARIES}
     ${HYSCAN_MODEL_LIBRARY})

add_executable (db-info-test db-info-test.c)
add_executable (hw-profile-test hw-profile-test.c)
add_executable (profile-loader-test profile-loader-test.c)
# add_executable (async-test async-test.c)
# add_executable (connector-test connector-test.c)
# add_executable (db-profile-test db-profile-test.c)
# add_executable (sonar-control-model-test sonar-control-model-test.c)
# add_executable (sonar-model-test sonar-model-test.c)
# add_executable (sonar-profile-test sonar-profile-test.c)

target_link_libraries (db-info-test ${TEST_LIBRARIES})
target_link_libraries (hw-profile-test ${TEST_LIBRARIES})
target_link_libraries (profile-loader-test ${TEST_LIBRARIES})
# target_link_libraries (async-test ${TEST_LIBRARIES})
# target_link_libraries (connector-test ${TEST_LIBRARIES})
# target_link_libraries (db-profile-test ${TEST_LIBRARIES})
# target_link_libraries (sonar-control-model-test ${TEST_LIBRARIES})
# target_link_libraries (sonar-model-test ${TEST_LIBRARIES})
# target_link_libraries (sonar-profile-test ${TEST_LIBRARIES})

set (HYSCAN_MODEL_TEST_INSTALL_TARGETS
     db-info-test
     hw-profile-test
     profile-loader-test
     # async-test
     # connector-test
     # db-profile-test
     # sonar-control-model-test
     # sonar-model-test
     # sonar-profile-test
     )

install (TARGETS ${HYSCAN_MODEL_TEST_INSTALL_TARGETS}
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

