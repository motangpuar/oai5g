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

/*! \file xnap_messages_types.h
 * \author Sreeshma Shiv <sreeshmau@iisc.ac.in>
 * \date August 2023
 * \version 1.0
 */
#ifndef XNAP_MESSAGES_TYPES_H_
#define XNAP_MESSAGES_TYPES_H_

#include "s1ap_messages_types.h"
#include "ngap_messages_types.h"
#include "NR_PhysCellId.h"
#include "asn_codecs_prim.h"
// Defines to access message fields.

#define XNAP_REGISTER_GNB_REQ(mSGpTR) (mSGpTR)->ittiMsg.xnap_register_gnb_req
#define XNAP_SETUP_REQ(mSGpTR) (mSGpTR)->ittiMsg.xnap_setup_req
#define XNAP_SETUP_RESP(mSGpTR) (mSGpTR)->ittiMsg.xnap_setup_resp
#define XNAP_SETUP_FAILURE(mSGpTR) (mSGpTR)->ittiMsg.xnap_setup_failure
#define XNAP_HANDOVER_REQ(mSGpTR) (mSGpTR)->ittiMsg.xnap_handover_req

#define XNAP_MAX_NB_GNB_IP_ADDRESS 4

// gNB application layer -> XNAP messages

typedef struct xnap_net_ip_address_s {
  unsigned ipv4:1;
  unsigned ipv6:1;
  char ipv4_address[16];
  char ipv6_address[46];
} xnap_net_ip_address_t;

<<<<<<< HEAD
typedef struct xnap_sctp_s {
=======
typedef struct xnap_register_gnb_req_s {
  uint32_t gNB_id;
  char *gNB_name;
  /* Tracking area code */
  uint16_t tac;

  /* Mobile Country Code
   * Mobile Network Code
   */
  uint16_t mcc;
  uint16_t mnc;
  uint8_t mnc_digit_length;
  int16_t eutra_band;
  int32_t nr_band;
  int32_t nrARFCN;
  uint32_t downlink_frequency;
  int32_t uplink_frequency_offset;
  uint32_t Nid_cell;
  int16_t N_RB_DL;
  frame_type_t frame_type;
  uint32_t fdd_earfcn_DL;
  uint32_t fdd_earfcn_UL;
  uint32_t subframeAssignment;
  uint32_t specialSubframe;
  uint16_t tdd_nRARFCN;
  uint16_t tdd_Transmission_Bandwidth;

  /* The local gNB IP address to bind */
  gnb_ip_address_t gnb_xn_ip_address;

  /* Nb of GNB to connect to */
  uint8_t nb_xn;

  /* List of target gNB to connect to for Xn*/
  gnb_ip_address_t target_gnb_xn_ip_address[XNAP_MAX_NB_GNB_IP_ADDRESS];

  /* Number of SCTP streams used for associations */
>>>>>>> b05e275c38 (xnap-targetgnb)
  uint16_t sctp_in_streams;
  uint16_t sctp_out_streams;
} xnap_sctp_t;

typedef struct xnap_net_config_t {
  uint8_t nb_xn;
  xnap_net_ip_address_t gnb_xn_ip_address;
  xnap_net_ip_address_t target_gnb_xn_ip_address[XNAP_MAX_NB_GNB_IP_ADDRESS];
  uint32_t gnb_port_for_XNC;
  xnap_sctp_t sctp_streams;
} xnap_net_config_t;

typedef struct xnap_plmn_t {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t  mnc_digit_length;
} xnap_plmn_t;

typedef struct xnap_amf_regioninfo_s {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t mnc_len;
  uint8_t amf_region_id;
} xnap_amf_regioninfo_t;

<<<<<<< HEAD
typedef enum xnap_mode_t { XNAP_MODE_TDD = 0, XNAP_MODE_FDD = 1 } xnap_mode_t;

typedef struct xnap_nr_frequency_info_t {
  uint32_t arfcn;
  int band;
} xnap_nr_frequency_info_t;

typedef struct xnap_transmission_bandwidth_t {
  uint8_t scs;
  uint16_t nrb;
} xnap_transmission_bandwidth_t;

typedef struct xnap_fdd_info_t {
  xnap_nr_frequency_info_t ul_freqinfo;
  xnap_nr_frequency_info_t dl_freqinfo;
  xnap_transmission_bandwidth_t ul_tbw;
  xnap_transmission_bandwidth_t dl_tbw;
} xnap_fdd_info_t;

typedef struct xnap_tdd_info_t {
  xnap_nr_frequency_info_t freqinfo;
  xnap_transmission_bandwidth_t tbw;
} xnap_tdd_info_t;

typedef struct xnap_snssai_s {
  uint8_t sst;
  uint8_t sd;
} xnap_snssai_t;

typedef struct xnap_served_cell_info_t {
  // NR CGI
  xnap_plmn_t plmn;
  uint64_t nr_cellid; // NR Global Cell Id
  uint16_t nr_pci;// NR Physical Cell Ids

  /* Tracking area code */
  uint32_t tac;

  xnap_mode_t mode;
  union {
    xnap_fdd_info_t fdd;
    xnap_tdd_info_t tdd;
  };

  char *measurement_timing_information;
} xnap_served_cell_info_t;

typedef struct xnap_setup_req_s {
  uint64_t gNB_id;
  /* Tracking area code */
  uint16_t num_tai;
  uint32_t tai_support;
  xnap_plmn_t plmn_support;
  // Number of slide support items
  uint16_t num_snssai;
  xnap_snssai_t snssai[2];
  xnap_amf_regioninfo_t amf_region_info;
  uint8_t num_cells_available;
  xnap_served_cell_info_t info;
} xnap_setup_req_t;

typedef struct xnap_setup_resp_s {
  int64_t gNB_id;
  /* Tracking area code */
  uint16_t num_tai;
  uint32_t tai_support;
  xnap_plmn_t plmn_support;
  // Number of slide support items
  uint16_t num_ssi;
  uint8_t sst;
  uint8_t sd;
  uint16_t nb_xn;//number of gNBs connected
  xnap_served_cell_info_t info;
} xnap_setup_resp_t;

typedef struct xnap_register_gnb_req_s {
  xnap_setup_req_t setup_req;
  xnap_net_config_t net_config;
  char *gNB_name;
} xnap_register_gnb_req_t;

typedef enum xnap_Cause_e {
  XNAP_CAUSE_NOTHING,  /* No components present */
  XNAP_CAUSE_RADIO_NETWORK,
  XNAP_CAUSE_TRANSPORT,
  XNAP_CAUSE_PROTOCOL,
  XNAP_CAUSE_MISC,
} xnap_Cause_t;

typedef struct xnap_setup_failure_s {
  long cause_value;
  xnap_Cause_t cause_type;
  uint16_t time_to_wait;
  uint16_t criticality_diagnostics;
} xnap_setup_failure_t;
=======
typedef struct xnap_lastvisitedcell_info_s {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t mnc_len;
  NR_PhysCellId_t target_physCellId;
  cell_type_t cell_type;
  uint64_t time_UE_StayedInCell;
} xnap_lastvisitedcell_info_t;

typedef struct xnap_allocation_retention_priority_s {
  ngap_priority_level_t priorityLevel;
  ngap_pre_emp_capability_t pre_emp_capability;
  ngap_pre_emp_vulnerability_t pre_emp_vulnerability;
} xnap_allocation_retention_priority_t;

typedef struct xnap_pdusession_level_qos_parameter_s {
  uint8_t qfi;
  long non_dynamic_fiveQI;
  long dynamic_priorityLevelQoS;
  long dynamic_packetDelayBudget;
  long dynamic_packetErrorRate_scalar;
  long dynamic_packetErrorRate_exponent;
  ngap_allocation_retention_priority_t allocation_retention_priority;
} xnap_pdusession_level_qos_parameter_t;

typedef struct xnap_pdusession_s {
  /* Unique pdusession_id for the UE. */
  uint8_t pdusession_id;
  transport_layer_addr_t gNB_addr;
  /* UPF Tunnel endpoint identifier */
  uint32_t gtp_teid;
  /* Quality of service for this pdusession */
  xnap_pdusession_level_qos_parameter_t qos[QOSFLOW_MAX_VALUE];
} xnap_pdusession_t;

typedef struct xnap_handover_req_s {
  /* RRC->XNAP in source eNB */
  int rnti;

  /* XNAP->RRC in target eNB */
  int xn_id;

  NR_PhysCellId_t target_physCellId;

  xnap_guami_t ue_guami;

  /*UE-ContextInformation */

  /* ? amf UE id  */
  ASN__PRIMITIVE_TYPE_t ue_ngap_id;
  security_capabilities_t security_capabilities;
  uint8_t kenb[32]; // keNB or keNB*

  /*next_hop_chaining_coun */
  long int kenb_ncc;

  /* UE aggregate maximum bitrate */
  ambr_t ue_ambr;

  uint8_t nb_pdu_resources_tobe_setup;

  /* list of pdu session to be setup by RRC layers */
  xnap_pdusession_t pdu_param[NGAP_MAX_PDUSESSION];

  xnap_lastvisitedcell_info_t lastvisitedcell_info;

  uint8_t rrc_buffer[8192 /* arbitrary, big enough */];
  int rrc_buffer_size;

  int target_assoc_id;
} xnap_handover_req_t;

>>>>>>> b05e275c38 (xnap-targetgnb)
#endif /* XNAP_MESSAGES_TYPES_H_ */
