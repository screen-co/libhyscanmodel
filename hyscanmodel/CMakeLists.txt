
add_library (${HYSCAN_MODEL_LIBRARY} SHARED
             hyscan-async.c
             hyscan-db-info.c
             hyscan-object-model.c)

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

install (FILES hyscan-async.h
               hyscan-db-info.h
               hyscan-object-model.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscanmodel"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
