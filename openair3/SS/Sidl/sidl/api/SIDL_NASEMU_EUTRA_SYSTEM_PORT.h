/*
 *****************************************************************
 *
 * Module  : SIDL - structures definitions
 *
 * Purpose : THIS FILE IS AUTOMATICALLY GENERATED !
 *
 *****************************************************************
 *
 *  Copyright (c) 2014-2021 SEQUANS Communications.
 *  All rights reserved.
 *
 *  This is confidential and proprietary source code of SEQUANS
 *  Communications. The use of the present source code and all
 *  its derived forms is exclusively governed by the restricted
 *  terms and conditions set forth in the SEQUANS
 *  Communications' EARLY ADOPTER AGREEMENT and/or LICENCE
 *  AGREEMENT. The present source code and all its derived
 *  forms can ONLY and EXCLUSIVELY be used with SEQUANS
 *  Communications' products. The distribution/sale of the
 *  present source code and all its derived forms is EXCLUSIVELY
 *  RESERVED to regular LICENCE holder and otherwise STRICTLY
 *  PROHIBITED.
 *
 *****************************************************************
 */

#pragma once

#include "SidlCompiler.h"
#include "SidlASN1.h"
#include "SidlASN1Base.h"
#include "SidlBase.h"
#include "SidlCommon.h"
#include "SidlCommonBase.h"
#include "SidlParts.h"
#include "SidlVals.h"
#include "TtcnCommon.h"
#include "SidlCommon_BcchConfig.h"
#include "SidlCommon_Bcch_BRConfig.h"
#include "SidlCommon_CcchDcchDtchConfig.h"
#include "SidlCommon_InitialCellPower.h"
#include "SidlCommon_PhysicalLayerConfigDL.h"
#include "SidlCommon_PhysicalLayerConfigUL.h"
#include "SidlCommon_RachProcedureConfig.h"
#include "SidlCommon_SciSchedulingConfig.h"
#include "SidlCommon_ServingCellConfig.h"
#include "SidlCommon_StaticCellInfo.h"
#include "SidlCommon_CellConfigRequest.h"

SIDL_BEGIN_C_INTERFACE

enum RRC_MSG_Request_Type_Sel {
	RRC_MSG_Request_Type_UNBOUND_VALUE = 0,
	RRC_MSG_Request_Type_Ccch = 1,
	RRC_MSG_Request_Type_Dcch = 2,
};

struct uint8_t_RRC_MSG_Request_Type_Ccch_Dynamic {
	size_t d;
	uint8_t* v;
};

struct uint8_t_RRC_MSG_Request_Type_Dcch_Dynamic {
	size_t d;
	uint8_t* v;
};

union RRC_MSG_Request_Type_Value {
	struct uint8_t_RRC_MSG_Request_Type_Ccch_Dynamic Ccch;
	struct uint8_t_RRC_MSG_Request_Type_Dcch_Dynamic Dcch;
};

struct RRC_MSG_Request_Type {
	enum RRC_MSG_Request_Type_Sel d;
	union RRC_MSG_Request_Type_Value v;
};

enum RRC_MSG_Indication_Type_Sel {
	RRC_MSG_Indication_Type_UNBOUND_VALUE = 0,
	RRC_MSG_Indication_Type_Ccch = 1,
	RRC_MSG_Indication_Type_Dcch = 2,
};

struct uint8_t_RRC_MSG_Indication_Type_Ccch_Dynamic {
	size_t d;
	uint8_t* v;
};

struct uint8_t_RRC_MSG_Indication_Type_Dcch_Dynamic {
	size_t d;
	uint8_t* v;
};

union RRC_MSG_Indication_Type_Value {
	struct uint8_t_RRC_MSG_Indication_Type_Ccch_Dynamic Ccch;
	struct uint8_t_RRC_MSG_Indication_Type_Dcch_Dynamic Dcch;
};

struct RRC_MSG_Indication_Type {
	enum RRC_MSG_Indication_Type_Sel d;
	union RRC_MSG_Indication_Type_Value v;
};

struct EUTRA_RRC_PDU_REQ {
	struct ReqAspCommonPart_Type Common;
	struct RRC_MSG_Request_Type RrcPdu;
};

struct EUTRA_RRC_PDU_IND {
	struct IndAspCommonPart_Type Common;
	struct RRC_MSG_Indication_Type RrcPdu;
};

SIDL_END_C_INTERFACE