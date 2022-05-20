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

SIDL_BEGIN_C_INTERFACE

typedef uint8_t Itrp_Type;

struct SL_Sci0Config_Type {
	struct UplinkHoppingControl_Type Hopping;
	struct FreqDomainSchedulExplicit_Type FreqDomainSchedul;
	Itrp_Type Itrp;
};

struct C_RNTI_SL_RNTI_r12_Optional {
	bool d;
	C_RNTI v;
};

struct N_PSCCH_Type_N_PSCCH_Optional {
	bool d;
	N_PSCCH_Type v;
};

struct SL_Sci0Config_Type_SL_Sheduling_Optional {
	bool d;
	struct SL_Sci0Config_Type v;
};

struct Sci0SchedulingConfig_Type {
	struct C_RNTI_SL_RNTI_r12_Optional SL_RNTI_r12;
	struct N_PSCCH_Type_N_PSCCH_Optional N_PSCCH;
	struct SL_Sci0Config_Type_SL_Sheduling_Optional SL_Sheduling;
};

struct SL_Sci1Config_Type {
	BIT_STRING FreqResourceAllocation;
	B4_Type TimeGap;
};

struct C_RNTI_SL_V_RNTI_r12_Optional {
	bool d;
	C_RNTI v;
};

struct B3_Type_CarrierIndicator_Optional {
	bool d;
	B3_Type v;
};

struct BIT_STRING_LowestIndexOfChannelAllocation_Optional {
	bool d;
	BIT_STRING v;
};

struct B2_Type_SL_Index_Optional {
	bool d;
	B2_Type v;
};

struct B3_Type_SL_SPS_ConfigurationIndex_Optional {
	bool d;
	B3_Type v;
};

struct B1_Type_SL_SPS_ActivationRelease_Optional {
	bool d;
	B1_Type v;
};

struct SL_Sci1Config_Type_SL_SCI1_Scheduling_Optional {
	bool d;
	struct SL_Sci1Config_Type v;
};

struct Sci1SchedulingConfig_Type {
	struct C_RNTI_SL_V_RNTI_r12_Optional SL_V_RNTI_r12;
	struct B3_Type_CarrierIndicator_Optional CarrierIndicator;
	struct BIT_STRING_LowestIndexOfChannelAllocation_Optional LowestIndexOfChannelAllocation;
	struct B2_Type_SL_Index_Optional SL_Index;
	struct B3_Type_SL_SPS_ConfigurationIndex_Optional SL_SPS_ConfigurationIndex;
	struct B1_Type_SL_SPS_ActivationRelease_Optional SL_SPS_ActivationRelease;
	struct SL_Sci1Config_Type_SL_SCI1_Scheduling_Optional SL_SCI1_Scheduling;
};

enum SciSchedulingConfig_Type_Sel {
	SciSchedulingConfig_Type_UNBOUND_VALUE = 0,
	SciSchedulingConfig_Type_SCI0 = 1,
	SciSchedulingConfig_Type_SCI1 = 2,
};

union SciSchedulingConfig_Type_Value {
	struct Sci0SchedulingConfig_Type SCI0;
	struct Sci1SchedulingConfig_Type SCI1;
};

struct SciSchedulingConfig_Type {
	enum SciSchedulingConfig_Type_Sel d;
	union SciSchedulingConfig_Type_Value v;
};

SIDL_END_C_INTERFACE