
set (TEST_LIBRARIES
     ${GLIB2_LIBRARIES}
     ${LIBXML2_LIBRARIES}
     ${HYSCAN_LIBRARIES}
     ${HYSCAN_MODEL_LIBRARY})

add_executable (db-info-test db-info-test.c)
add_executable (track-stats-test track-stats-test.c waypoint.c)
add_executable (planner-stats-test planner-stats-test.c waypoint.c)
# add_executable (async-test async-test.c)
# add_executable (connector-test connector-test.c)
# add_executable (sonar-control-model-test sonar-control-model-test.c)
# add_executable (sonar-model-test sonar-model-test.c)

target_link_libraries (db-info-test ${TEST_LIBRARIES})
target_link_libraries (track-stats-test ${TEST_LIBRARIES})
target_link_libraries (planner-stats-test ${TEST_LIBRARIES})
# target_link_libraries (async-test ${TEST_LIBRARIES})
# target_link_libraries (connector-test ${TEST_LIBRARIES})
# target_link_libraries (sonar-control-model-test ${TEST_LIBRARIES})
# target_link_libraries (sonar-model-test ${TEST_LIBRARIES})

set (HYSCAN_MODEL_TEST_INSTALL_TARGETS
     db-info-test
     track-stats-test
     planner-stats-test
     # async-test
     # connector-test
     # sonar-control-model-test
     # sonar-model-test
     )

install (TARGETS ${HYSCAN_MODEL_TEST_INSTALL_TARGETS}
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

file (REMOVE_RECURSE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/db")
file (MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/db")
add_test (NAME DBInfoTest COMMAND db-info-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
#add_test (NAME AsyncTest COMMAND async-test
#          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME TrackStatsTest COMMAND track-stats-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_test (NAME PlannerStatsTest COMMAND planner-stats-test file://db
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
