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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "intertask_interface.h"
#include "assertions.h"
#include "conversions.h"
#include "xnap_common.h"
#include "xnap_gNB_defs.h"
#include "xnap_gNB_task.h"

// xnap_gNB_internal_data_t xnap_gNB_internal_data;
static xnap_gNB_instance_t *xn_inst[NUMBER_OF_gNB_MAX] = {0};

static int xnap_gNB_compare_assoc_id(const xnap_gNB_data_t *p1, const xnap_gNB_data_t *p2)
{
  if (p1->assoc_id > p2->assoc_id)
    return 1;
  if (p1->assoc_id == p2->assoc_id)
    return 0;
  return -1; /* p1->assoc_id < p1->assoc_id */
}

RB_GENERATE(xnap_gnb_tree, xnap_gNB_data_t, entry, xnap_gNB_compare_assoc_id);

/*int xnap_gNB_compare_assoc_id(struct xnap_gNB_data_s *p1, struct xnap_gNB_data_s *p2)
{
  if (p1->assoc_id == -1) {
    if (p1->cnx_id < p2->cnx_id) {
      return -1;
    }

    if (p1->cnx_id > p2->cnx_id) {
      return 1;
    }
  } else {
    if (p1->assoc_id < p2->assoc_id) {
      return -1;
    }

    if (p1->assoc_id > p2->assoc_id) {
      return 1;
    }
  }
  return 0;
}*/

static pthread_mutex_t xn_inst_mtx = PTHREAD_MUTEX_INITIALIZER;
void createXninst(instance_t instanceP, xnap_setup_req_t *req, xnap_net_config_t *nc)
{
  DevAssert(instanceP == 0);
  pthread_mutex_lock(&xn_inst_mtx);
  AssertFatal(xn_inst[0] == NULL, "Attempted to initialize multiple Xn instances\n");
  xn_inst[0] = calloc(1, sizeof(xnap_gNB_instance_t));
  AssertFatal(xn_inst[0] != NULL, "out of memory\n");
  if (req)
    xn_inst[0]->setup_req = *req;
  if (nc)
    xn_inst[0]->net_config = *nc;
  pthread_mutex_unlock(&xn_inst_mtx);
}

void xnap_dump_trees(const instance_t instance)
{
  pthread_mutex_lock(&xn_inst_mtx);
  printf("%ld connected gNBs \n", xn_inst[instance]->num_gnbs);
  xnap_gNB_data_t *xnap_gnb_data_p = NULL;
  /* cast is necessary to eliminate warning "discards ‘const’ qualifier" */
  RB_FOREACH (xnap_gnb_data_p, xnap_gnb_tree, &((xnap_gNB_instance_t *)xn_inst[instance])->xnap_gnbs) {
    if (xnap_gnb_data_p->assoc_id == -1) {
      printf("integrated gNB");
      printf("cnx_id %d\n", xnap_gnb_data_p->cnx_id);
    } else {
      printf("assoc_id %d", xnap_gnb_data_p->assoc_id);
      printf("state %d\n", xnap_gnb_data_p->state);
      printf("cnx_id %d\n", xnap_gnb_data_p->cnx_id);
    }
  }
  pthread_mutex_unlock(&xn_inst_mtx);
}

xnap_gNB_data_t *xnap_get_gNB(instance_t instance, sctp_assoc_t assoc_id)
{
  AssertFatal(assoc_id != 0, "illegal assoc_id == 0: should be -1 or >0)\n");
  xnap_gNB_data_t e = {.assoc_id = assoc_id};
  pthread_mutex_lock(&xn_inst_mtx);
  xnap_gNB_data_t *xnap_gnb = RB_FIND(xnap_gnb_tree, &xn_inst[instance]->xnap_gnbs, &e);
  if (xnap_gnb == NULL) {
    LOG_W(NR_RRC, "no gNB connected or not found for assoc_id %d: Xn Setup Failed?\n", assoc_id);
    pthread_mutex_unlock(&xn_inst_mtx);
    return NULL;
  }
  pthread_mutex_unlock(&xn_inst_mtx);
  return xnap_gnb;
}

xnap_gNB_instance_t *xnap_gNB_get_instance(instance_t instanceP)
{
  DevAssert(instanceP == 0);
  pthread_mutex_lock(&xn_inst_mtx);
  xnap_gNB_instance_t *instance_xnap = xn_inst[instanceP];
  pthread_mutex_unlock(&xn_inst_mtx);
  return instance_xnap;
}

void xnap_insert_gnb(instance_t instance, xnap_gNB_data_t *xnap_gnb_data_p)
{
  pthread_mutex_lock(&xn_inst_mtx);
  RB_INSERT(xnap_gnb_tree, &xn_inst[instance]->xnap_gnbs, xnap_gnb_data_p);
  xn_inst[instance]->num_gnbs++;
  pthread_mutex_unlock(&xn_inst_mtx);
}

void xnap_handle_xn_setup_message(instance_t instance, sctp_assoc_t assoc_id, int sctp_shutdown)
{
  if (sctp_shutdown) {
    /* A previously connected gNB has been shutdown */
    xnap_gNB_data_t *gnb_data_p = xnap_get_gNB(instance, assoc_id);
    if (gnb_data_p == NULL) {
      LOG_W(XNAP, "no gNB connected or not found for assoc_id %d:\n", assoc_id);
      return;
    }
    pthread_mutex_lock(&xn_inst_mtx);
    if (gnb_data_p->state == XNAP_GNB_STATE_CONNECTED) {
      gnb_data_p->state = XNAP_GNB_STATE_DISCONNECTED;
      // Removing the gNB data from tree
      RB_REMOVE(xnap_gnb_tree, &xn_inst[instance]->xnap_gnbs, gnb_data_p);
      if (xn_inst[instance]->xn_target_gnb_associated_nb > 0) {
        /* Decrease associated gNB number */
        xn_inst[instance]->xn_target_gnb_associated_nb--;
      }

      /* If there are no more associated gNB */
      if (xn_inst[instance]->xn_target_gnb_associated_nb == 0) {
        // TODO : Inform GNB_APP ???
        LOG_I(XNAP, "No more associated gNBs- Number of connected gNBS : %d", xn_inst[instance]->xn_target_gnb_associated_nb);
      }
    }
  } else {
    /* Check that at least one setup message is pending */
    DevCheck(xn_inst[instance]->xn_target_gnb_pending_nb > 0, instance, xn_inst[instance]->xn_target_gnb_pending_nb, 0);

    if (xn_inst[instance]->xn_target_gnb_pending_nb > 0) {
      /* Decrease pending messages number */
      xn_inst[instance]->xn_target_gnb_pending_nb--;
    }

    /* If there are no more pending messages  */
    if (xn_inst[instance]->xn_target_gnb_pending_nb == 0) {
      // TODO : Need to inform GNB_APP??
      LOG_I(XNAP, "No more pending messages- Number of connected gNBS : %d", xn_inst[instance]->xn_target_gnb_associated_nb);
    }
  }
  pthread_mutex_unlock(&xn_inst_mtx);
}

/*xnap_gNB_data_t *xnap_is_gNB_id_in_list(const uint32_t gNB_id)
{
  xnap_gNB_instance_t *inst;
  struct xnap_gNB_data_s *elm;

  STAILQ_FOREACH(inst, &xnap_gNB_internal_data.xnap_gNB_instances_head, xnap_gNB_entries)
  {
    RB_FOREACH(elm, xnap_gnb_map, &inst->xnap_gnb_head)
    {
      if (elm->gNB_id == gNB_id)
        return elm;
    }
  }
  return NULL;
}*/
