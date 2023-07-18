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

/*! \file xnap_gNB_handler.c
 * \brief xnap handler procedures for gNB
 * \author Sreeshma Shiv <sreeshmau@iisc.ac.in>
 * \date August 2023
 * \version 1.0
 */

#include <stdint.h>
#include "intertask_interface.h"
#include "xnap_common.h"
#include "xnap_gNB_defs.h"
#include "xnap_gNB_handler.h"
#include "xnap_gNB_interface_management.h"
#include "assertions.h"
#include "conversions.h"

/* Placement of callback functions according to XNAP_ProcedureCode.h */
static const xnap_message_decoded_callback xnap_messages_callback[][3] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {xnap_gNB_handle_xn_setup_request, xnap_gNB_handle_xn_setup_response, xnap_gNB_handle_xn_setup_failure}, /* xnSetup */
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}};

static const char *const xnap_direction_String[] = {
    "", /* Nothing */
    "Originating message", /* originating message */
    "Successfull outcome", /* successfull outcome */
    "UnSuccessfull outcome", /* successfull outcome */
};
const char *xnap_direction2String(int xnap_dir)
{
  return (xnap_direction_String[xnap_dir]);
}

int xnap_gNB_handle_message(instance_t instance,
                            sctp_assoc_t assoc_id,
                            int32_t stream,
                            const uint8_t *const data,
                            const uint32_t data_length)
{
  XNAP_XnAP_PDU_t pdu;
  int ret = 0;

  DevAssert(data != NULL);

  memset(&pdu, 0, sizeof(pdu));

  printf("Data length received: %d\n", data_length);
  if (xnap_gNB_decode_pdu(&pdu, data, data_length) < 0) {
    LOG_E(XNAP, "Failed to decode PDU\n");
    return -1;
  }

  switch (pdu.present) {
    case XNAP_XnAP_PDU_PR_initiatingMessage:
      LOG_I(XNAP, "xnap_gNB_decode_initiating_message!\n");
      /* Checking procedure Code and direction of message */
      if (pdu.choice.initiatingMessage->procedureCode
          >= sizeof(xnap_messages_callback) / (3 * sizeof(xnap_message_decoded_callback))) {
        //|| (pdu.present > XNAP_XnAP_PDU_PR_unsuccessfulOutcome)) {
        LOG_E(XNAP, "[SCTP %d] Either procedureCode %ld exceed expected\n", assoc_id, pdu.choice.initiatingMessage->procedureCode);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }

      /* No handler present */
      if (xnap_messages_callback[pdu.choice.initiatingMessage->procedureCode][pdu.present - 1] == NULL) {
        LOG_E(XNAP,
              "[SCTP %d] No handler for procedureCode %ld in %s\n",
              assoc_id,
              pdu.choice.initiatingMessage->procedureCode,
              xnap_direction2String(pdu.present - 1));
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }
      /* Calling the right handler */
      ret =
          (*xnap_messages_callback[pdu.choice.initiatingMessage->procedureCode][pdu.present - 1])(instance, assoc_id, stream, &pdu);
      break;

    case XNAP_XnAP_PDU_PR_successfulOutcome:
      LOG_I(XNAP, "xnap_gNB_decode_successfuloutcome_message!\n");
      /* Checking procedure Code and direction of message */
      if (pdu.choice.successfulOutcome->procedureCode
          >= sizeof(xnap_messages_callback) / (3 * sizeof(xnap_message_decoded_callback))) {
        LOG_E(XNAP, "[SCTP %d] Either procedureCode %ld exceed expected\n", assoc_id, pdu.choice.successfulOutcome->procedureCode);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }

      /* No handler present.*/
      if (xnap_messages_callback[pdu.choice.successfulOutcome->procedureCode][pdu.present - 1] == NULL) {
        LOG_E(XNAP,
              "[SCTP %d] No handler for procedureCode %ld in %s\n",
              assoc_id,
              pdu.choice.successfulOutcome->procedureCode,
              xnap_direction2String(pdu.present - 1));
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }
      /* Calling the right handler */
      ret =
          (*xnap_messages_callback[pdu.choice.successfulOutcome->procedureCode][pdu.present - 1])(instance, assoc_id, stream, &pdu);
      break;

    case XNAP_XnAP_PDU_PR_unsuccessfulOutcome:
      LOG_I(XNAP, "xnap_gNB_decode_unsuccessfuloutcome_message!\n");
      /* Checking procedure Code and direction of message */
      if (pdu.choice.unsuccessfulOutcome->procedureCode
          >= sizeof(xnap_messages_callback) / (3 * sizeof(xnap_message_decoded_callback))) {
        LOG_E(XNAP,
              "[SCTP %d] Either procedureCode %ld exceed expected\n",
              assoc_id,
              pdu.choice.unsuccessfulOutcome->procedureCode);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }

      /* No handler present */
      if (xnap_messages_callback[pdu.choice.unsuccessfulOutcome->procedureCode][pdu.present - 1] == NULL) {
        LOG_E(XNAP,
              "[SCTP %d] No handler for procedureCode %ld in %s\n",
              assoc_id,
              pdu.choice.unsuccessfulOutcome->procedureCode,
              xnap_direction2String(pdu.present - 1));
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }
      /* Calling the right handler */
      ret = (*xnap_messages_callback[pdu.choice.unsuccessfulOutcome->procedureCode][pdu.present - 1])(instance,
                                                                                                      assoc_id,
                                                                                                      stream,
                                                                                                      &pdu);
      break;

    default:
      LOG_E(XNAP, "[SCTP %d] Direction %d exceed expected\n", assoc_id, pdu.present);
      break;
  }

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
  return ret;
}
