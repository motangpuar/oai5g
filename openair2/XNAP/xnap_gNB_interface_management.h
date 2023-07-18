/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file xnap_gNB_interface_management.h
 * \brief xnap interface handler procedures for gNB
 * \date 2023 Dec
 * \version 1.0
 */

#ifndef XNAP_GNB_INTERFACE_MANAGEMENT_H_
#define XNAP_GNB_INTERFACE_MANAGEMENT_H_

/*Xn Setup*/
int xnap_gNB_handle_xn_setup_request(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu);
int xnap_gNB_handle_xn_setup_response(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu);
int xnap_gNB_handle_xn_setup_failure(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu);

#endif /* XNAP_GNB_INTERFACE_MANAGEMENT_H_ */
