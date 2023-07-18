add_subdirectory(MESSAGES)
add_library(xnap 
              xnap_common.c 
              xnap_gNB_task.c
              xnap_gNB_management_procedures.c 
              xnap_gNB_itti_messaging.c
              xnap_gNB_handler.c
              xnap_gNB_interface_management.c
              xnap_gNB_generate_messages)
target_link_libraries(xnap
                        PUBLIC asn1_xnap
                        PRIVATE nr_rrc)
target_include_directories(xnap PUBLIC ${CMAKE_CURRENT_DIR})

