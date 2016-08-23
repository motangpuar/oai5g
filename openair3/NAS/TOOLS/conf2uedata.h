#ifndef _CONF2UEDATA_H
#define _CONF2UEDATA_H

#include <libconfig.h>

#include "usim_api.h"

#define UE "UE"

#define PLMN "PLMN"

#define FULLNAME "FULLNAME"
#define SHORTNAME "SHORTNAME"
#define MNC "MNC"
#define MCC "MCC"

#define HPLMN "HPLMN"
#define UCPLMN "UCPLMN_LIST"
#define OPLMN "OPLMN_LIST"
#define OCPLMN "OCPLMN_LIST"
#define FPLMN "FPLMN_LIST"
#define EHPLMN "EHPLMN_LIST"

#define MIN_TAC     0x0000
#define MAX_TAC     0xFFFE

/*
 * PLMN network operator record
 */
typedef struct {
  unsigned int num;
  plmn_t plmn;
  char fullname[NET_FORMAT_LONG_SIZE + 1];
  char shortname[NET_FORMAT_SHORT_SIZE + 1];
  tac_t tac_start;
  tac_t tac_end;
} network_record_t;

typedef struct {
	const char *fullname;
	const char *shortname;
	const char *mnc;
	const char *mcc;
} plmn_conf_param_t;

typedef struct {
    int size;
    int *items;
} plmns_list;

extern plmns_list ucplmns;
extern plmns_list oplmns;
extern plmns_list ocplmns;
extern plmns_list fplmns;
extern plmns_list ehplmns;

extern int plmn_nb;

extern plmn_conf_param_t* user_plmn_list;
extern network_record_t* user_network_record_list;

int get_config_from_file(const char *filename, config_t *config);
int parse_config_file(const char *output_dir, const char *filename);

void _display_usage(void);
void fill_network_record_list(void);

int parse_plmn_param(config_setting_t *plmn_setting, int index);
int parse_plmns(config_setting_t *all_plmn_setting);
int get_plmn_index(const char * mccmnc);
int parse_ue_plmn_param(config_setting_t *ue_setting, int user_id, const char **hplmn);
int parse_Xplmn(config_setting_t *ue_setting, const char *section,
               int user_id, plmns_list *plmns);


#endif // _CONF2UEDATA_H
