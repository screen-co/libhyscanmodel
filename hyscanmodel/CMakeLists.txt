set (HYSCAN_MODEL_LIBRARY_SOURCE_FILES
     hyscan-db-info.c
     hyscan-profile.c
     hyscan-db-profile.c
     hyscan-hw-profile-device.c
     hyscan-hw-profile.c
     hyscan-offset-profile.c
     hyscan-profile-loader.c
     # hyscan-appearance.c
     # hyscan-async.c
     # hyscan-connector.c
     # hyscan-profile-common.c
     # hyscan-sensors-data.c
     # hyscan-sensors-profile.c
     # hyscan-sonar-control-model.c
     # hyscan-sonar-model.c
     # hyscan-sonar-profile.c
     # hyscan-serializable.c
     )

add_library (${HYSCAN_MODEL_LIBRARY} SHARED ${HYSCAN_MODEL_LIBRARY_SOURCE_FILES})

target_link_libraries (${HYSCAN_MODEL_LIBRARY} ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (${HYSCAN_MODEL_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_MODEL_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_MODEL_VERSION})

configure_file (hyscanmodel.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscanmodel-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_MODEL_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscanmodel-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "lib/pkgconfig"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

set (HYSCAN_MODEL_LIBRARY_INSTALL_FILES
     hyscan-db-info.h
     hyscan-db-profile.h
     # hyscan-appearance.h
     # hyscan-async.h
     # hyscan-connector.h
     # hyscan-profile-common.h
     # hyscan-sensors-data.h
     # hyscan-sensors-profile.h
     # hyscan-sonar-control-model.h
     # hyscan-sonar-model.h
     # hyscan-sonar-profile.h
     # hyscan-serializable.h
     )

install (FILES ${HYSCAN_MODEL_LIBRARY_INSTALL_FILES}
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscanmodel"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
