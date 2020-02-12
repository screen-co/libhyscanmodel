set (HYSCAN_MODEL_LIBRARY_SOURCE_FILES
     hyscan-db-info.c
     hyscan-async.c
     hyscan-sonar-state.c
     hyscan-sensor-state.c
     hyscan-sonar-model.c
     hyscan-model-manager.c
     hyscan-mark-location.c
     hyscan-mark-loc-model.c
     # hyscan-appearance.c
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
     hyscan-async.h
     hyscan-sonar-state.h
     hyscan-sensor-state.h
     hyscan-sonar-model.h
     hyscan-model-manager.h
     hyscan-mark-location.h
     hyscan-mark-loc-model.h
     # hyscan-appearance.h
     # hyscan-serializable.h
     )

install (FILES ${HYSCAN_MODEL_LIBRARY_INSTALL_FILES}
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscanmodel"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
