/* Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
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

/*! \file XNAP/xnap_gNB_task.c
 * \brief XNAP tasks and functions definitions
 * \author Sreeshma Shiv
 * \date Aug 2023
 * \version 1.0
 * \email: sreeshmau@iisc.ac.in
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "intertask_interface.h"
#include <openair3/ocp-gtpu/gtp_itf.h>
#include "xnap_gNB_task.h"
#include "xnap_gNB_defs.h"
#include "xnap_gNB_management_procedures.h"
#include "xnap_gNB_handler.h"
#include "xnap_gNB_generate_messages.h"
#include "queue.h"
#include "assertions.h"
#include "conversions.h"

// struct xnap_gnb_map;
// struct xnap_gNB_data_s;

RB_PROTOTYPE(xnap_gnb_tree, xnap_gNB_data_t, entry, xnap_gNB_compare_assoc_id);

static void xnap_gNB_handle_sctp_data_ind(instance_t instance, sctp_data_ind_t *sctp_data_ind);

static void xnap_gNB_handle_sctp_association_ind(instance_t instance, sctp_new_association_ind_t *sctp_new_association_ind);

// static void xnap_gNB_handle_register_gNB(instance_t instance, xnap_register_gnb_req_t *xnap_register_gNB);

static void xnap_gNB_send_sctp_assoc_req(instance_t instance_p, xnap_net_config_t *nc, int index);
static void xnap_gNB_handle_sctp_association_resp(instance_t instance, sctp_new_association_resp_t *sctp_new_association_resp);

static void xnap_gNB_handle_sctp_data_ind(instance_t instance, sctp_data_ind_t *sctp_data_ind)
{
  int result;
  DevAssert(sctp_data_ind != NULL);
  xnap_gNB_handle_message(instance,
                          sctp_data_ind->assoc_id,
                          sctp_data_ind->stream,
                          sctp_data_ind->buffer,
                          sctp_data_ind->buffer_length);
  result = itti_free(TASK_UNKNOWN, sctp_data_ind->buffer);
  AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
}

static void xnap_gNB_handle_sctp_association_resp(instance_t instance, sctp_new_association_resp_t *sctp_new_association_resp)
{
  xnap_gNB_instance_t *instance_xn = xnap_gNB_get_instance(instance); // managementproc;
  DevAssert(sctp_new_association_resp != NULL);
  DevAssert(instance_xn != NULL);
  printf("reached ass resp %d \n", sctp_new_association_resp->assoc_id);
  /*Return if connection to gNB failed- to be modified if needed. (Exit on error in X2AP)*/
  if (sctp_new_association_resp->sctp_state == SCTP_STATE_UNREACHABLE) {
    LOG_E(XNAP,
          "association with gNB failed, is it running? If no, run it first. If yes, check IP addresses in your configuration "
          "file.\n");
    return;
  }
  if (sctp_new_association_resp->sctp_state != SCTP_STATE_ESTABLISHED) {
    LOG_W(XNAP,
          "Received unsuccessful result for SCTP association state (%u), assoc_id (%d), instance %ld, cnx_id %u, retrying...\n",
          sctp_new_association_resp->sctp_state,
          sctp_new_association_resp->assoc_id,
          instance,
          sctp_new_association_resp->ulp_cnx_id);
    xnap_handle_xn_setup_message(instance,
                                 sctp_new_association_resp->assoc_id,
                                 sctp_new_association_resp->sctp_state == SCTP_STATE_SHUTDOWN);
    // sleep(3);
    // xnap_gNB_send_sctp_assoc_req(instance, &instance_xn->net_config, sctp_new_association_resp->ulp_cnx_id);
    return; // exit -1 for debugging
  }

  xnap_gNB_data_t *xnap_gnb_data_p = calloc(1, sizeof(*xnap_gnb_data_p));
  AssertFatal(xnap_gnb_data_p != NULL, "out of memory\n");
  xnap_gnb_data_p->cnx_id = sctp_new_association_resp->ulp_cnx_id;
  xnap_gnb_data_p->assoc_id = sctp_new_association_resp->assoc_id;
  xnap_gnb_data_p->state = XNAP_GNB_STATE_WAITING;
  xnap_gnb_data_p->in_streams = sctp_new_association_resp->in_streams;
  xnap_gnb_data_p->out_streams = sctp_new_association_resp->out_streams;
  xnap_dump_trees(instance);
  xnap_insert_gnb(instance, xnap_gnb_data_p);
  // RB_INSERT(xnap_gnb_tree, &instance_xn->xnap_gnbs, xnap_gnb_data_p);
  // instance_xn->num_gnbs++;
  xnap_dump_trees(instance);
  xnap_gNB_generate_xn_setup_request(sctp_new_association_resp->assoc_id, &instance_xn->setup_req);
}

int xnap_gNB_init_sctp(instance_t instance_p, xnap_net_config_t *nc)
{
  // Create and alloc new message
  MessageDef *message;
  sctp_init_t *sctp_init = NULL;
  DevAssert(nc != NULL);
  message = itti_alloc_new_message(TASK_XNAP, 0, SCTP_INIT_MSG_MULTI_REQ);
  sctp_init = &message->ittiMsg.sctp_init_multi;
  sctp_init->port = nc->gnb_port_for_XNC;
  sctp_init->ppid = XNAP_SCTP_PPID;
  sctp_init->ipv4 = 1;
  sctp_init->ipv6 = 0;
  sctp_init->nb_ipv4_addr = 1;
#if 0
  memcpy(&sctp_init->ipv4_address,
         nc->gnb_xn_ip_address,
         sizeof(*nc->gnb_xn_ip_address));
#endif
  sctp_init->ipv4_address[0] = inet_addr(nc->gnb_xn_ip_address.ipv4_address);
  sctp_init->nb_ipv6_addr = 0;
  sctp_init->ipv6_address[0] = "0:0:0:0:0:0:0:1";
  return itti_send_msg_to_task(TASK_SCTP, instance_p, message);
}

static void xnap_gNB_send_sctp_assoc_req(instance_t instance, xnap_net_config_t *nc, int index)
{
  MessageDef *message = NULL;
  sctp_new_association_req_t *sctp_new_association_req = NULL;
  DevAssert(nc != NULL);
  printf("blahblahblah \n");
  message = itti_alloc_new_message(TASK_XNAP, 0, SCTP_NEW_ASSOCIATION_REQ);
  printf("ghjfghjjdfhj\n");
  sctp_new_association_req = &message->ittiMsg.sctp_new_association_req;
  sctp_new_association_req->port = nc->gnb_port_for_XNC;
  sctp_new_association_req->ppid = XNAP_SCTP_PPID;
  sctp_new_association_req->in_streams = nc->sctp_streams.sctp_in_streams;
  sctp_new_association_req->out_streams = nc->sctp_streams.sctp_out_streams;

  memcpy(&sctp_new_association_req->remote_address,
         &nc->target_gnb_xn_ip_address[index],
         sizeof(nc->target_gnb_xn_ip_address[index]));
  memcpy(&sctp_new_association_req->local_address, &nc->gnb_xn_ip_address, sizeof(nc->gnb_xn_ip_address));
  sctp_new_association_req->ulp_cnx_id = index;
  // instance_p->xn_target_gnb_pending_nb++;
  itti_send_msg_to_task(TASK_SCTP, instance, message);
}

static void xnap_gNB_handle_sctp_init_msg_multi_cnf(instance_t instance, sctp_init_msg_multi_cnf_t *m)
{
  xnap_gNB_instance_t *instance_xn = xnap_gNB_get_instance(instance);
  DevAssert(m != NULL);
  DevAssert(instance_xn != NULL);
  // instance->multi_sd = m->multi_sd;

  /* Exit if CNF message reports failure.
   * Failure means multi_sd < 0.
   */
  if (m->multi_sd < 0) {
    LOG_E(XNAP, "Error: be sure to properly configure XN in your configuration file.\n");
    DevAssert(m->multi_sd >= 0);
  }

  /* Trying to connect to the provided list of gNB ip address */

  for (int index = 0; index < instance_xn->net_config.nb_xn; index++) {
    LOG_I(XNAP, "gNB[%ld] gNB id %lx index %d acting as an initiator (client)\n", instance, instance_xn->setup_req.gNB_id, index);
    instance_xn->xn_target_gnb_pending_nb++;
    xnap_gNB_send_sctp_assoc_req(instance, &instance_xn->net_config, index);
  }
}

static void xnap_gNB_handle_sctp_association_ind(instance_t instance, sctp_new_association_ind_t *sctp_new_association_ind)
{
  // f1ap_cudu_inst_t *f1ap_cu_data = getCxt(instance);
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);
  DevAssert(instance_p != NULL);
  xnap_gNB_data_t *xnap_gnb_data_p;
  DevAssert(sctp_new_association_ind != NULL);
  // we don't need the assoc_id, subsequent messages (the first being Xn Setup
  // Request), will deliver the assoc_id
  // f1ap_cu_data->sctp_in_streams  = sctp_new_association_ind->in_streams;
  // f1ap_cu_data->sctp_out_streams = sctp_new_association_ind->out_streams;
  LOG_W(XNAP, "SCTP Association IND Received.\n");
  xnap_dump_trees(instance);
  xnap_gnb_data_p = xnap_get_gNB(instance, sctp_new_association_ind->assoc_id);

  //  DevAssert(xnap_gnb_data_p != NULL);
  if (xnap_gnb_data_p == NULL) {
    LOG_W(XNAP, "xnap_gnb_data_p does not exist\n");
    /* TODO: Create new gNB descriptor-not yet associated? */
    xnap_gNB_data_t *xnap_gnb_data_p = calloc(1, sizeof(*xnap_gnb_data_p));
    AssertFatal(xnap_gnb_data_p != NULL, "out of memory\n");
    xnap_gnb_data_p->assoc_id = sctp_new_association_ind->assoc_id;
    xnap_gnb_data_p->state = XNAP_GNB_STATE_WAITING;
    printf("reached here1\n");
    xnap_gnb_data_p->in_streams = sctp_new_association_ind->in_streams;
    xnap_gnb_data_p->out_streams = sctp_new_association_ind->out_streams;
    xnap_dump_trees(instance);
    xnap_insert_gnb(instance, xnap_gnb_data_p);
    // RB_INSERT(xnap_gnb_tree, &instance_xn->xnap_gnbs, xnap_gnb_data_p);
    // instance_xn->num_gnbs++;
    xnap_dump_trees(instance);
  } else {
    xnap_gnb_data_p->in_streams = sctp_new_association_ind->in_streams;
    xnap_gnb_data_p->out_streams = sctp_new_association_ind->out_streams;
    LOG_W(XNAP, "Updated streams for assoc id: %d \n", sctp_new_association_ind->assoc_id);
  }
  xnap_dump_trees(instance);
}

void *xnap_task(void *arg)
{
  MessageDef *received_msg = NULL;
  int result;
  LOG_D(XNAP, "Starting XNAP layer\n");
  // xnap_gNB_prepare_internal_data(); // management procedures
  itti_mark_task_ready(TASK_XNAP);

  while (1) {
    itti_receive_msg(TASK_XNAP, &received_msg);
    LOG_D(XNAP, "Received message %d:%s\n", ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
    switch (ITTI_MSG_ID(received_msg)) {
      case TERMINATE_MESSAGE:
        LOG_W(XNAP, "Exiting XNAP thread\n");
        itti_exit_task();
        break;

      case XNAP_REGISTER_GNB_REQ: {
        xnap_setup_req_t *xnsetupmsg = &XNAP_REGISTER_GNB_REQ(received_msg).setup_req;
        xnap_net_config_t *xn_nc = &XNAP_REGISTER_GNB_REQ(received_msg).net_config;
        createXninst(ITTI_MSG_DESTINATION_INSTANCE(received_msg), xnsetupmsg, xn_nc);
        xnap_gNB_init_sctp(ITTI_MSG_DESTINATION_INSTANCE(received_msg), xn_nc);
      } break;

      case XNAP_SETUP_FAILURE: // from rrc/xnap
        xnap_gNB_generate_xn_setup_failure(ITTI_MSG_ORIGIN_INSTANCE(received_msg), &XNAP_SETUP_FAILURE(received_msg));
        break;

      case XNAP_SETUP_RESP: // from rrc
        xnap_gNB_generate_xn_setup_response(ITTI_MSG_ORIGIN_INSTANCE(received_msg), &XNAP_SETUP_RESP(received_msg));
        break;

      case SCTP_INIT_MSG_MULTI_CNF:
        xnap_gNB_handle_sctp_init_msg_multi_cnf(ITTI_MSG_DESTINATION_INSTANCE(received_msg),
                                                &received_msg->ittiMsg.sctp_init_msg_multi_cnf);
        break;

      case SCTP_NEW_ASSOCIATION_RESP:
        xnap_gNB_handle_sctp_association_resp(ITTI_MSG_DESTINATION_INSTANCE(received_msg),
                                              &received_msg->ittiMsg.sctp_new_association_resp);
        break;

      case SCTP_NEW_ASSOCIATION_IND:
        xnap_gNB_handle_sctp_association_ind(ITTI_MSG_DESTINATION_INSTANCE(received_msg),
                                             &received_msg->ittiMsg.sctp_new_association_ind);
        break;

      case SCTP_DATA_IND:
        xnap_gNB_handle_sctp_data_ind(ITTI_MSG_DESTINATION_INSTANCE(received_msg), &received_msg->ittiMsg.sctp_data_ind);
        break;

      default:
        LOG_E(XNAP, "Received unhandled message: %d:%s\n", ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
        break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(received_msg), received_msg);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    received_msg = NULL;
  }

  return NULL;
}

#include "common/config/config_userapi.h"

int is_xnap_enabled(void)
{
  static volatile int config_loaded = 0;
  static volatile int enabled = 0;
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  if (pthread_mutex_lock(&mutex))
    goto mutex_error;
  if (config_loaded) {
    if (pthread_mutex_unlock(&mutex))
      goto mutex_error;
    return enabled;
  }

  char *enable_xn = NULL;
  paramdef_t p[] = {{"enable_xn", "yes/no", 0, .strptr = &enable_xn, .defstrval = "", TYPE_STRING, 0}};

  /* TODO: do it per module - we check only first gNB */
  config_get(config_get_if(), p, sizeofArray(p), "gNBs.[0]");
  if (enable_xn != NULL && strcmp(enable_xn, "yes") == 0) {
    enabled = 1;
  }

  /*Consider also the case of enabling XnAP for a gNB by parsing a gNB configuration file*/

  config_get(config_get_if(), p, sizeofArray(p), "gNBs.[0]");
  if (enable_xn != NULL && strcmp(enable_xn, "yes") == 0) {
    enabled = 1;
  }

  config_loaded = 1;

  if (pthread_mutex_unlock(&mutex))
    goto mutex_error;
  return enabled;

mutex_error:
  LOG_E(XNAP, "mutex error\n");
  exit(1);
}
