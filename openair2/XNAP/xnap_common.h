/* Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 *file except in compliance with the License. You may obtain a copy of the
 *License at
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

#ifndef XNAP_COMMON_H_
#define XNAP_COMMON_H_
#include "XNAP_InitiatingMessage.h"
#include "XNAP_ProtocolExtensionContainer.h"
#include "XNAP_ProtocolExtensionField.h"
#include "XNAP_ProtocolIE-ContainerPair.h"
#include "XNAP_ProtocolIE-Field.h"
#include "XNAP_ProtocolIE-FieldPair.h"
#include "XNAP_SuccessfulOutcome.h"
#include "XNAP_UnsuccessfulOutcome.h"
#include "XNAP_XnAP-PDU.h"
#include "XNAP_asn_constant.h"
#include "common/openairinterface5g_limits.h"
#include "intertask_interface.h"
#include "oai_asn1.h"

#ifndef XNAP_PORT
#define XNAP_PORT 38423
#endif

extern int asn1_xer_print;

#define XNAP_FIND_PROTOCOLIE_BY_ID(IE_TYPE, ie, container, IE_ID, mandatory)                                                   \
  do {                                                                                                                         \
    IE_TYPE **ptr;                                                                                                             \
    ie = NULL;                                                                                                                 \
    for (ptr = container->protocolIEs.list.array; ptr < &container->protocolIEs.list.array[container->protocolIEs.list.count]; \
         ptr++) {                                                                                                              \
      if ((*ptr)->id == IE_ID) {                                                                                               \
        ie = *ptr;                                                                                                             \
        break;                                                                                                                 \
      }                                                                                                                        \
    }                                                                                                                          \
    if (mandatory)                                                                                                             \
      DevAssert(ie != NULL);                                                                                                   \
  } while (0)

typedef int (*xnap_message_decoded_callback)(instance_t instance, sctp_assoc_t assocId, uint32_t stream, XNAP_XnAP_PDU_t *pdu);
int xnap_gNB_decode_pdu(XNAP_XnAP_PDU_t *pdu, const uint8_t *const buffer, uint32_t length) __attribute__((warn_unused_result));
int xnap_gNB_encode_pdu(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len) __attribute__((warn_unused_result));
#endif /* XNAP_COMMON_H_ */
