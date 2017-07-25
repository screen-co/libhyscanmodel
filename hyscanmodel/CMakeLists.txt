
add_library (hyscanmodel SHARED
             hyscan-db-info.c
             hyscan-async.c)

target_link_libraries (hyscanmodel ${GLIB2_LIBRARIES} ${HYSCAN_LIBRARIES})

set_target_properties (hyscanmodel PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
