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

/*! \file xnap_common.c
 * \brief xnap encoder,decoder dunctions for gNB
 * \author Sreeshma Shiv <sreeshmau@iisc.ac.in>
 * \date Dec 2023
 * \version 1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "assertions.h"
#include "conversions.h"
#include "intertask_interface.h"
#include "xnap_common.h"

int xnap_gNB_encode_pdu(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
{
  ssize_t encoded;

  DevAssert(pdu != NULL);
  DevAssert(buffer != NULL);
  DevAssert(len != NULL);

  xer_fprint(stdout, &asn_DEF_XNAP_XnAP_PDU, (void *)pdu);

  encoded = aper_encode_to_new_buffer(&asn_DEF_XNAP_XnAP_PDU, 0, pdu, (void **)buffer);

  if (encoded < 0) {
    return -1;
  }

  *len = encoded;

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, pdu);
  return encoded;
}

int xnap_gNB_decode_pdu(XNAP_XnAP_PDU_t *pdu, const uint8_t *const buffer, uint32_t length)
{
  asn_dec_rval_t dec_ret;

  DevAssert(buffer != NULL);

  dec_ret = aper_decode(NULL, &asn_DEF_XNAP_XnAP_PDU, (void **)&pdu, buffer, length, 0, 0);
  xer_fprint(stdout, &asn_DEF_XNAP_XnAP_PDU, pdu);
  if (dec_ret.code != RC_OK) {
    LOG_E(XNAP, "Failed to decode PDU\n");
    return -1;
  }
  return 0;
}
