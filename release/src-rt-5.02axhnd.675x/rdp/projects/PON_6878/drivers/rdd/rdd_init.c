/*
* <:copyright-BRCM:2014:proprietary:standard
* 
*    Copyright (c) 2014 Broadcom 
*    All Rights Reserved
* 
*  This program is the proprietary software of Broadcom and/or its
*  licensors, and may only be used, duplicated, modified or distributed pursuant
*  to the terms and conditions of a separate, written license agreement executed
*  between you and Broadcom (an "Authorized License").  Except as set forth in
*  an Authorized License, Broadcom grants no license (express or implied), right
*  to use, or waiver of any kind with respect to the Software, and Broadcom
*  expressly reserves all rights in and to the Software and all intellectual
*  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
*  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
*  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
* 
*  Except as expressly set forth in the Authorized License,
* 
*  1. This program, including its structure, sequence and organization,
*     constitutes the valuable trade secrets of Broadcom, and you shall use
*     all reasonable efforts to protect the confidentiality thereof, and to
*     use this information only in connection with your use of Broadcom
*     integrated circuit products.
* 
*  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*     AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*     WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
*     RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
*     ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
*     FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
*     COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
*     TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
*     PERFORMANCE OF THE SOFTWARE.
* 
*  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
*     ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
*     INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
*     WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
*     IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
*     OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
*     SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
*     SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
*     LIMITED REMEDY.
* :>
*/


#include "rdd.h"
#include "rdd_defs.h"
#include "rdd_init.h"
#include "rdd_tunnels_parsing.h"
#ifndef _CFE_
#ifdef CONFIG_DHD_RUNNER
#include "rdd_dhd_helper.h"
#include "rdp_drv_dhd.h"
#endif
#endif
#include "rdd_iptv_processing.h"
#include "rdd_qos_mapper.h"
#include "XRDP_AG.h"
#include "rdd_map_auto.h"
#include "rdd_ghost_reporting.h"
#include "rdd_scheduling.h"
#include "rdd_common.h"
#include "rdd_cpu.h"
#include "rdp_common.h"
#include "rdd_tuple_lkp.h"
#include "rdd_runner_proj_defs.h"
#ifndef _CFE_
#ifdef CONFIG_DHD_RUNNER
#include "rdd_dhd_helper.h"
#include "rdp_drv_dhd.h"
#endif
#include "rdd_tcam_ic.h"
#include "rdd_ingress_filter.h"
#include "rdd_iptv.h"
#include "rdd_dscp_to_pbit.h"
#include "rdd_bridge.h"
#include "rdp_drv_rnr.h"
#endif
#include "rdp_drv_qm.h"
#include "rdd_ag_natc.h"
#include "rdd_service_queues.h"
#include "rdd_debug.h"


extern uintptr_t xrdp_virt2phys(const ru_block_rec *ru_block, uint8_t addr_idx);
static void rdd_proj_init(rdd_init_params_t *init_params);
static void rdd_actions_proj_init(void);
static void rdd_tm_actions_proj_init(void);
static void rdd_service_queues_actions_proj_init(void);

#ifdef RDP_SIM
extern uint8_t *soc_base_address;
extern uint32_t natc_lkp_table_ptr;
extern int rdd_sim_alloc_segments(void);
extern void rdd_sim_free_segments(void);
#endif

extern int reg_id[32];
#ifndef _CFE_
DEFINE_BDMF_FASTLOCK(int_lock);
DEFINE_BDMF_FASTLOCK(iptv_lock);
DEFINE_BDMF_FASTLOCK(int_lock_irq);
#endif

#ifdef USE_BDMF_SHELL
extern int rdd_make_shell_commands(void);
#endif /* USE_BDMF_SHELL */

rdd_bb_id rdpa_emac_to_bb_id_rx[rdpa_emac__num_of] = {
    BB_ID_RX_BBH_0,
    BB_ID_RX_BBH_1,
    BB_ID_RX_BBH_2,
    BB_ID_RX_BBH_3,
    BB_ID_RX_BBH_4,
    BB_ID_RX_PON,
    BB_ID_LAST,
};

rdd_bb_id rdpa_emac_to_bb_id_tx[rdpa_emac__num_of] = {
    BB_ID_TX_LAN,
    BB_ID_TX_PON_ETH_PD,
    BB_ID_LAST,
};

rdd_vport_id_t rx_flow_to_vport[RX_FLOW_CONTEXTS_NUMBER] = {
    [0 ... RDD_MAX_RX_WAN_FLOW]  = RDD_WAN0_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_RX_BBH_0] = RDD_LAN0_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_RX_BBH_1] = RDD_LAN1_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_RX_BBH_2] = RDD_LAN2_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_RX_BBH_3] = RDD_LAN3_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_RX_BBH_4] = RDD_LAN4_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_CPU0] = RDD_CPU0_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_CPU1] = RDD_CPU1_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_CPU2] = RDD_CPU2_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_CPU4] = RDD_CPU4_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_CPU5] = RDD_CPU5_VPORT,
    [RDD_NUM_OF_RX_WAN_FLOWS + BB_ID_CPU6] = RDD_CPU6_VPORT,
};

bbh_id_e rdpa_emac_to_bbh_id_e[rdpa_emac__num_of] = {
    BBH_ID_0,
    BBH_ID_1,
    BBH_ID_2,
    BBH_ID_3,
    BBH_ID_4,
    BBH_ID_NULL,
};

extern RDD_FPM_GLOBAL_CFG_DTS g_fpm_hw_cfg;

int rdd_init(void)
{
#ifdef RDP_SIM
    if (rdd_sim_alloc_segments())
        return -1;
#endif
    return 0;
}

void rdd_exit(void)
{
#ifdef RDP_SIM
    rdd_sim_free_segments();
#endif
}

#ifndef RDP_SIM
void rdp_rnr_write_context(void *__to, void *__from, unsigned int __n);
#endif

static void rdd_global_registers_init(uint32_t core_index, uint32_t local_regs[NUM_OF_MAIN_RUNNER_THREADS][NUM_OF_LOCAL_REGS], uint32_t last_thread)
{
     uint32_t i;

    /********** Reserved global registers **********/
    /* R6 - ingress qos don't drop counter in processing cores */

    /********** common to all runners **********/
    /* in this project we don't have really global, so will set all registers that should be global for all threads */
    for (i = 0; i <= last_thread; ++i)
    {

        local_regs[i][31] = 1; /* CONST_1 is 1, in 6878 its r31 */
        local_regs[i][2] = RDD_CPU_RECYCLE_SRAM_PD_FIFO_ADDRESS_ARR[core_index]; /* See usage in fw_runner_defs.h */
        
        /* VPORT_CFG_EX address is here just to save a mov command in FW and can be replaced if necessary */
        local_regs[i][4] = (RDD_VPORT_CFG_EX_TABLE_ADDRESS_ARR[core_index] << 16) + RDD_VPORT_CFG_TABLE_ADDRESS_ARR[core_index];
    }
}

static void image_0_context_set(uint32_t core_index, rdd_init_params_t *init_params)
{
    static uint32_t local_regs[NUM_OF_MAIN_RUNNER_THREADS][NUM_OF_LOCAL_REGS];
    uint32_t *sram_context;

    sram_context = (uint32_t *)RUNNER_CORE_CONTEXT_ADDRESS(core_index);
#ifndef XRDP_EMULATION
    /* read the local registers from the Context memory - maybe it was initialized by the ACE compiler */
     MREAD_BLK_32(local_regs, sram_context, sizeof(local_regs));
#endif
    rdd_global_registers_init(core_index, local_regs, IMAGE_0_TM_LAST);
	
	/* DIRECT PROCESSING : thread 0 */
    local_regs[IMAGE_0_TM_WAN_DIRECT_THREAD_NUMBER][reg_id[9]] = IMAGE_0_DIRECT_PROCESSING_FLOW_CNTR_TABLE_ADDRESS << 16 | IMAGE_0_TM_WAN_DIRECT_THREAD_NUMBER;
    local_regs[IMAGE_0_TM_WAN_DIRECT_THREAD_NUMBER][reg_id[10]] = BB_ID_DISPATCHER_REORDER;
    local_regs[IMAGE_0_TM_WAN_DIRECT_THREAD_NUMBER][reg_id[11]] = IMAGE_0_US_TM_WAN_0_BB_DESTINATION_TABLE_ADDRESS << 16 | QM_QUEUE_CPU_RX_COPY_EXCLUSIVE;
    local_regs[IMAGE_0_TM_WAN_DIRECT_THREAD_NUMBER][reg_id[0]] =  ADDRESS_OF(image_0, gpon_control_wakeup_request);
    local_regs[IMAGE_0_TM_WAN_DIRECT_THREAD_NUMBER][reg_id[16]] = IMAGE_0_DIRECT_PROCESSING_PD_TABLE_ADDRESS;

    /* EPON UPDATE_FIFO_READ: thread 1 */
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_EPON_THREAD_NUMBER][reg_id[17]] = drv_qm_get_us_start() & 0x1f;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_EPON_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, epon_update_fifo_read_1st_wakeup_request);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_EPON_THREAD_NUMBER][reg_id[12]] = BB_ID_TX_PON_ETH_PD;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_EPON_THREAD_NUMBER][reg_id[11]] = IMAGE_0_EPON_UPDATE_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_EPON_THREAD_NUMBER][reg_id[10]] = BB_ID_QM_RNR_GRID;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_EPON_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_US_TM_SCHEDULING_QUEUE_TABLE_ADDRESS;

    /* EPON SCHEDULING WAN: thread 2 */
    local_regs[IMAGE_0_TM_WAN_EPON_THREAD_NUMBER][reg_id[17]] = drv_qm_get_us_epon_start();
    local_regs[IMAGE_0_TM_WAN_EPON_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, epon_tx_task_wakeup_request);
    local_regs[IMAGE_0_TM_WAN_EPON_THREAD_NUMBER][reg_id[12]] = BB_ID_TX_PON_ETH_PD | (IMAGE_0_BBH_TX_EPON_EGRESS_COUNTER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_WAN_EPON_THREAD_NUMBER][reg_id[11]] = IMAGE_0_US_TM_PD_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_WAN_EPON_THREAD_NUMBER][reg_id[10]] = BB_ID_QM_RNR_GRID | (IMAGE_0_US_TM_SCHEDULING_QUEUE_TABLE_ADDRESS << 16); 
    local_regs[IMAGE_0_TM_WAN_EPON_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_BBH_TX_EPON_INGRESS_COUNTER_TABLE_ADDRESS;

    /* UPDATE_FIFO_READ: thread 3 */
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[17]] = drv_qm_get_us_start() & 0x1f;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, update_fifo_us_read_1st_wakeup_request);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[14]] = IMAGE_0_COMPLEX_SCHEDULER_TABLE_ADDRESS | (IMAGE_0_US_TM_SCHEDULING_QUEUE_AGING_VECTOR_ADDRESS << 16);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[12]] = (IMAGE_0_US_TM_PD_FIFO_TABLE_ADDRESS << 16);
#ifdef DQM_SW_WORKAROUND
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[11]] = IMAGE_0_US_TM_UPDATE_FIFO_TABLE_ADDRESS | (IMAGE_0_US_TM_WAN_0_BB_DESTINATION_TABLE_ADDRESS << 16);
#else
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[11]] = IMAGE_0_US_TM_UPDATE_FIFO_TABLE_ADDRESS;
#endif
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[10]] = BB_ID_QM_RNR_GRID | (IMAGE_0_US_TM_BBH_QUEUE_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_US_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_US_TM_SCHEDULING_QUEUE_TABLE_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_US_ADDRESS << 16);

    /* SCHEDULING WAN_0: thread 4 */
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, us_tx_task_1st_wakeup_request);
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[16]] = IMAGE_0_US_TM_WAN_0_BBH_TX_WAKE_UP_DATA_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[12]] = (IMAGE_0_US_TM_WAN_0_BBH_TX_EGRESS_COUNTER_TABLE_ADDRESS << 16) | IMAGE_0_WAN_0_BBH_TX_FIFO_SIZE_ADDRESS;
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[11]] = (IMAGE_0_COMPLEX_SCHEDULER_TABLE_ADDRESS << 16);  /* low register used dynamically */
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[10]] = IMAGE_0_US_TM_PD_FIFO_TABLE_ADDRESS | (IMAGE_0_BASIC_RATE_LIMITER_TABLE_US_ADDRESS << 16);
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[9]]  = (IMAGE_0_US_TM_SCHEDULING_QUEUE_TABLE_ADDRESS << 16) | IMAGE_0_US_TM_WAN_0_BB_DESTINATION_TABLE_ADDRESS; 
    local_regs[IMAGE_0_TM_WAN_THREAD_NUMBER][reg_id[8]]  = IMAGE_0_US_TM_BBH_QUEUE_TABLE_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_US_ADDRESS << 16);

    /* REPORTING : thread 5 */
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[18]] = drv_qm_get_us_end();
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[17]] = XGPON_MAC_REPORT_ADDRESS;
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, ghost_reporting_1st_wakeup_request);
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[15]] = xrdp_virt2phys(&RU_BLK(QM), 0) + RU_REG_OFFSET(QM, TOTAL_VALID_COUNTER_COUNTER);
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[14]] = BB_ID_TX_PON_ETH_STAT;
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[13]] = IMAGE_0_REPORTING_QUEUE_DESCRIPTOR_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[12]] = IMAGE_0_REPORTING_QUEUE_ACCUMULATED_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[11]] = IMAGE_0_GHOST_REPORTING_QUEUE_STATUS_BIT_VECTOR_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[10]] = xrdp_virt2phys(&RU_BLK(QM), 0) + RU_REG_OFFSET(QM, EPON_RPT_CNT_COUNTER);
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[9]] = xrdp_virt2phys(&RU_BLK(QM), 0) + RU_REG_OFFSET(QM, EPON_RPT_CNT_QUEUE_STATUS);
    local_regs[IMAGE_0_TM_REPORTING_THREAD_NUMBER][reg_id[8]] = IMAGE_0_REPORTING_COUNTER_TABLE_ADDRESS;

    /* FLUSH TASK: thread 6 */
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[19]] = drv_qm_get_ds_start();
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[18]] = (((drv_qm_get_us_end() - drv_qm_get_us_start() + 8) / 8) << 16) +  ((((drv_qm_get_ds_end() - (drv_qm_get_ds_start() &~ 0x1F)) + 8)/ 8));
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[17]] = (BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_TM_FLUSH << 6));
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, flush_task_1st_wakeup_request);
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[15]] = IMAGE_0_DS_TM_PD_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[14]] = IMAGE_0_DS_TM_SCHEDULING_QUEUE_AGING_VECTOR_ADDRESS;
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[13]] = ((drv_qm_get_ds_start() - (drv_qm_get_ds_start() % 32)) / 8);


    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[11]] = IMAGE_0_US_TM_PD_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[10]] = IMAGE_0_US_TM_SCHEDULING_QUEUE_AGING_VECTOR_ADDRESS;
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[9]] = IMAGE_0_SCHEDULING_AGGREGATION_CONTEXT_VECTOR_ADDRESS;
    local_regs[IMAGE_0_TM_FLUSH_THREAD_NUMBER][reg_id[8]] = xrdp_virt2phys(&RU_BLK(QM), 0) + RU_REG_OFFSET(QM, GLOBAL_CFG_AGGREGATION_CONTEXT_VALID); /*(US is always 0, DS is used by pointer to correct location in r13*/

    /* Budget allocation: thread 7 */
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, budget_allocator_us_1st_wakeup_request);
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[13]] = FFI_32_SIZE * sizeof(RDD_BASIC_RATE_LIMITER_DESCRIPTOR_DTS); /* The task use ffi32 so the size is 32 * entry */
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[12]] = IMAGE_0_US_TM_SCHEDULING_QUEUE_TABLE_ADDRESS | (IMAGE_0_COMPLEX_SCHEDULER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[10]] = IMAGE_0_RATE_LIMITER_VALID_TABLE_US_ADDRESS;
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_BASIC_RATE_LIMITER_TABLE_US_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_US_ADDRESS << 16);

    /* Budget allocation: thread 8 */
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_DS_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, budget_allocator_ds_1st_wakeup_request);
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_DS_THREAD_NUMBER][reg_id[13]] = FFI_32_SIZE * sizeof(RDD_BASIC_RATE_LIMITER_DESCRIPTOR_DTS); /* The task use ffi32 so the size is 32 * entry */
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_DS_THREAD_NUMBER][reg_id[12]] = IMAGE_0_DS_TM_SCHEDULING_QUEUE_TABLE_ADDRESS | (IMAGE_0_COMPLEX_SCHEDULER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_DS_THREAD_NUMBER][reg_id[11]] = BB_ID_TX_LAN;
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_DS_THREAD_NUMBER][reg_id[10]] = IMAGE_0_RATE_LIMITER_VALID_TABLE_DS_ADDRESS;
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATION_DS_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_BASIC_RATE_LIMITER_TABLE_DS_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_DS_ADDRESS << 16);

    /* Overall budget allocation (for ovlr rl): thread 9 */
    local_regs[IMAGE_0_TM_OVL_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[8]] = IMAGE_0_US_TM_BBH_QUEUE_TABLE_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_US_ADDRESS << 16);
    local_regs[IMAGE_0_TM_OVL_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[11]] = (IMAGE_0_COMPLEX_SCHEDULER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_OVL_BUDGET_ALLOCATION_US_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, ovl_budget_allocator_1st_wakeup_request);

    /* UPDATE_FIFO_READ: thread 10 */
    local_regs[IMAGE_0_TM_UPDATE_FIFO_DS_THREAD_NUMBER][reg_id[17]] = drv_qm_get_ds_start() & 0x1f;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_DS_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, update_fifo_ds_read_1st_wakeup_request);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_DS_THREAD_NUMBER][reg_id[14]] = IMAGE_0_COMPLEX_SCHEDULER_TABLE_ADDRESS | (IMAGE_0_DS_TM_SCHEDULING_QUEUE_AGING_VECTOR_ADDRESS << 16);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_DS_THREAD_NUMBER][reg_id[11]] = IMAGE_0_DS_TM_UPDATE_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_UPDATE_FIFO_DS_THREAD_NUMBER][reg_id[10]] = BB_ID_QM_RNR_GRID | (IMAGE_0_DS_TM_BBH_QUEUE_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_UPDATE_FIFO_DS_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_DS_TM_SCHEDULING_QUEUE_TABLE_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_DS_ADDRESS << 16);

    /* SCHEDULING LAN: thread 11 */
    local_regs[IMAGE_0_TM_TX_TASK_DS_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, ds_tx_task_wakeup_request);
    local_regs[IMAGE_0_TM_TX_TASK_DS_THREAD_NUMBER][reg_id[12]] = (IMAGE_0_DS_TM_BBH_TX_EGRESS_COUNTER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_TX_TASK_DS_THREAD_NUMBER][reg_id[11]] = BB_ID_TX_LAN;
    local_regs[IMAGE_0_TM_TX_TASK_DS_THREAD_NUMBER][reg_id[10]] = IMAGE_0_DS_TM_PD_FIFO_TABLE_ADDRESS | (IMAGE_0_BASIC_RATE_LIMITER_TABLE_DS_ADDRESS << 16);
    local_regs[IMAGE_0_TM_TX_TASK_DS_THREAD_NUMBER][reg_id[9]] = (IMAGE_0_DS_TM_SCHEDULING_QUEUE_TABLE_ADDRESS << 16); 
    local_regs[IMAGE_0_TM_TX_TASK_DS_THREAD_NUMBER][reg_id[8]]  = IMAGE_0_DS_TM_BBH_QUEUE_TABLE_ADDRESS | (IMAGE_0_BASIC_SCHEDULER_TABLE_DS_ADDRESS << 16);

    /* TIMER_COMMON: thread 12 */
    local_regs[IMAGE_0_TM_TIMER_COMMON_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, timer_common_task_wakeup_request);
    local_regs[IMAGE_0_TM_TIMER_COMMON_THREAD_NUMBER][reg_id[14]] = CNTR_MAX_VAL;

    /* UPDATE_FIFO_READ: thread 13 */
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[17]] = drv_qm_get_sq_start() & 0x1f;
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, service_queues_update_fifo_read_1st_wakeup_request);
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[14]] = IMAGE_0_SERVICE_QUEUES_COMPLEX_SCHEDULER_TABLE_ADDRESS | (IMAGE_0_SERVICE_QUEUES_SCHEDULING_QUEUE_AGING_VECTOR_ADDRESS << 16);
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[13]] = (((IMAGE_0_TM_SERVICE_QUEUES_TX_THREAD_NUMBER) << 4) + 1);
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[11]] = IMAGE_0_SERVICE_QUEUES_UPDATE_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[10]] = BB_ID_QM_RNR_GRID ;
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_UPDATE_FIFO_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_SERVICE_QUEUES_SCHEDULING_QUEUE_TABLE_ADDRESS;

    /* SERVICE QUEUES SCHEDULING TX: thread 14 */
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_TX_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, service_queues_tx_task_wakeup_request);
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_TX_THREAD_NUMBER][reg_id[11]] = (IMAGE_0_SERVICE_QUEUES_COMPLEX_SCHEDULER_TABLE_ADDRESS << 16) | IMAGE_0_SERVICE_QUEUES_PD_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_TX_THREAD_NUMBER][reg_id[10]] = (IMAGE_0_SERVICE_QUEUES_RATE_LIMITER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_SERVICE_QUEUES_TX_THREAD_NUMBER][reg_id[9]]  = (IMAGE_0_SERVICE_QUEUES_SCHEDULING_QUEUE_TABLE_ADDRESS << 16) | (BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_SERVICE_QUEUES << 6)) ;

    /* SERVICE QUEUES BUDGET ALLOCATOR: thread 15 */
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_0, budget_allocator_1st_wakeup_request);
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[13]] = FFI_32_SIZE * sizeof(RDD_BASIC_RATE_LIMITER_DESCRIPTOR_DTS); /* The task use ffi32 so the size is 32 * entry */
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[12]] = IMAGE_0_SERVICE_QUEUES_SCHEDULING_QUEUE_TABLE_ADDRESS | (IMAGE_0_SERVICE_QUEUES_COMPLEX_SCHEDULER_TABLE_ADDRESS << 16);
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[10]] = IMAGE_0_SERVICE_QUEUES_RATE_LIMITER_VALID_TABLE_ADDRESS;
    local_regs[IMAGE_0_TM_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[9]]  = IMAGE_0_SERVICE_QUEUES_RATE_LIMITER_TABLE_ADDRESS;

#if defined(RDP_SIM) || defined(XRDP_EMULATION) 
    /* copy the local registers initial values to the Context memory */
    MWRITE_BLK_32(sram_context, local_regs, sizeof(local_regs));
#else
     rdp_rnr_write_context(sram_context, local_regs, sizeof(local_regs));
#endif
}

static void image_1_context_set(uint32_t core_index, rdd_init_params_t *init_params)
{
    static uint32_t local_regs[NUM_OF_MAIN_RUNNER_THREADS][NUM_OF_LOCAL_REGS];
    uint32_t *sram_context;
    uint32_t task;


    sram_context = (uint32_t *)RUNNER_CORE_CONTEXT_ADDRESS(core_index);
#ifndef XRDP_EMULATION
    /* read the local registers from the Context memory - maybe it was initialized by the ACE compiler */
    MREAD_BLK_32(local_regs, sram_context, sizeof(local_regs));
#endif
    rdd_global_registers_init(core_index, local_regs, IMAGE_1_PROCESSING_LAST);
    
    ag_drv_rnr_regs_cfg_ext_acc_cfg_set(core_index, PACKET_BUFFER_PD_PTR(IMAGE_1_DS_PACKET_BUFFER_ADDRESS, 0), 9, 8);

    /* CPU_RX_READ: thread 0 */
    if (init_params->fw_clang_dis)
    {
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, cpu_rx_wakeup_request);
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[13]] = IMAGE_1_UPDATE_FIFO_TABLE_ADDRESS;
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[12]] = IMAGE_1_CPU_RING_DESCRIPTORS_TABLE_ADDRESS;
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[11]] = IMAGE_1_PD_FIFO_TABLE_ADDRESS;
    }
    else
    {
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, c_cpu_rx_wakeup_request);
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[17]] = IMAGE_1_PD_FIFO_TABLE_ADDRESS;
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[18]] = IMAGE_1_UPDATE_FIFO_TABLE_ADDRESS;
      local_regs[IMAGE_1_PROCESSING_CPU_RX_THREAD_NUMBER][reg_id[30]] = IMAGE_1_CPU_RX_TASK_STACK_ADDRESS;
    }

    /* CPU_RX_METER_BUDGET_ALLOCATOR: thread 1 */
    local_regs[IMAGE_1_PROCESSING_CPU_RX_METER_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[0]] =  ADDRESS_OF(image_1, cpu_rx_meter_budget_allocator_1st_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_CPU_RX_METER_BUDGET_ALLOCATOR_THREAD_NUMBER][reg_id[11]] = CPU_RX_METER_TIMER_PERIOD_IN_USEC;

    /* CPU_RECYCLE: thread 2 */
    local_regs[IMAGE_1_PROCESSING_CPU_RECYCLE_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, cpu_recycle_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_CPU_RECYCLE_THREAD_NUMBER][reg_id[8]] = IMAGE_1_CPU_RECYCLE_SRAM_PD_FIFO_ADDRESS; 

    /* CPU_INTERRUPT_COALESCING: thread 3 */
    local_regs[IMAGE_1_PROCESSING_INTERRUPT_COALESCING_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, interrupt_coalescing_1st_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_INTERRUPT_COALESCING_THREAD_NUMBER][reg_id[13]] = IMAGE_1_CPU_INTERRUPT_COALESCING_TABLE_ADDRESS;
    local_regs[IMAGE_1_PROCESSING_INTERRUPT_COALESCING_THREAD_NUMBER][reg_id[12]] = IMAGE_1_CPU_RING_DESCRIPTORS_TABLE_ADDRESS | (IMAGE_1_CPU_RING_INTERRUPT_COUNTER_TABLE_ADDRESS << 16);
    
    /* CPU_RX_COPY_READ: thread 4 */
    local_regs[IMAGE_1_PROCESSING_CPU_RX_COPY_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, cpu_rx_copy_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_CPU_RX_COPY_THREAD_NUMBER][reg_id[14]] = IMAGE_1_CPU_RX_SCRATCHPAD_ADDRESS |
        (IMAGE_1_CPU_RX_PSRAM_GET_NEXT_SCRATCHPAD_ADDRESS << 16);
    local_regs[IMAGE_1_PROCESSING_CPU_RX_COPY_THREAD_NUMBER][reg_id[13]] = IMAGE_1_CPU_RX_COPY_UPDATE_FIFO_TABLE_ADDRESS | (IMAGE_1_WAN_LOOPBACK_DISPATCHER_CREDIT_TABLE_ADDRESS << 16);
    local_regs[IMAGE_1_PROCESSING_CPU_RX_COPY_THREAD_NUMBER][reg_id[12]] = 0;
    local_regs[IMAGE_1_PROCESSING_CPU_RX_COPY_THREAD_NUMBER][reg_id[11]] = (IMAGE_1_CPU_RX_COPY_PD_FIFO_TABLE_ADDRESS) | (IMAGE_1_CPU_RX_COPY_DISPATCHER_CREDIT_TABLE_ADDRESS << 16);

#if defined(CONFIG_BCM_SPDSVC_SUPPORT) || defined(CONFIG_BCM_TCPSPDTEST_SUPPORT) || defined(CONFIG_BCM_UDPSPDTEST_SUPPORT)
    /* Common reprocessing task is used tx_abs_recycle for speed service, or pktgen_reprocessing for tcp speed test.
     * It will be also used to recycle host buffers from CPU TX once CPU TX from ABS address is implemented */

    /* COMMON_REPROCESSING: thread 5 */
    local_regs[IMAGE_1_PROCESSING_COMMON_REPROCESSING_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, common_reprocessing_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_COMMON_REPROCESSING_THREAD_NUMBER][reg_id[16]] = IMAGE_1_COMMON_REPROCESSING_UPDATE_FIFO_TABLE_ADDRESS;
    local_regs[IMAGE_1_PROCESSING_COMMON_REPROCESSING_THREAD_NUMBER][reg_id[10]] = IMAGE_1_COMMON_REPROCESSING_DISPATCHER_CREDIT_TABLE_ADDRESS |
        ((BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_COMMON_REPROCESSING << 6)) << 16);

#endif

#if defined(CONFIG_BCM_SPDSVC_SUPPORT) || defined(CONFIG_BCM_UDPSPDTEST_SUPPORT)
    /* SPEED SERVICE: thread 6 */
    local_regs[IMAGE_1_PROCESSING_SPDSVC_GEN_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, spdsvc_gen_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_SPDSVC_GEN_THREAD_NUMBER][reg_id[9]] = ((BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_SPDSVC << 6)) << 16);
    local_regs[IMAGE_1_PROCESSING_SPDSVC_GEN_THREAD_NUMBER][reg_id[11]] = (IMAGE_1_SPDSVC_GEN_DISPATCHER_CREDIT_TABLE_ADDRESS << 16);
#endif

    /* PROCESSING : thread 7-12 */
    for (task = IMAGE_1_PROCESSING_PROCESSING0_THREAD_NUMBER; task <= IMAGE_1_PROCESSING_PROCESSING5_THREAD_NUMBER; task++) 
    {
        local_regs[task][reg_id[9]] = IMAGE_1_RX_FLOW_TABLE_ADDRESS << 16 | task;
        local_regs[task][reg_id[0]] = ADDRESS_OF(image_1, processing_wakeup_request);
        local_regs[task][reg_id[16]] = PACKET_BUFFER_PD_PTR(IMAGE_1_DS_PACKET_BUFFER_ADDRESS, (task-IMAGE_1_PROCESSING_PROCESSING0_THREAD_NUMBER));
    }

#ifndef _CFE_
#ifdef CONFIG_DHD_RUNNER
    /* DHD_TX_COMPLETE_0: thread 13 */
    local_regs[IMAGE_1_PROCESSING_DHD_TX_COMPLETE_0_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, dhd_tx_complete_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_DHD_TX_COMPLETE_0_THREAD_NUMBER][reg_id[13]] = (BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_DHD_TX_COMPLETE_0 << 6));
    local_regs[IMAGE_1_PROCESSING_DHD_TX_COMPLETE_0_THREAD_NUMBER][reg_id[12]] = (IMAGE_1_DHD_COMPLETE_COMMON_RADIO_DATA_ADDRESS + sizeof(RDD_DHD_COMPLETE_COMMON_RADIO_ENTRY_DTS)*0) | (IMAGE_1_DHD_TX_COMPLETE_0_DISPATCHER_CREDIT_TABLE_ADDRESS << 16);
    local_regs[IMAGE_1_PROCESSING_DHD_TX_COMPLETE_0_THREAD_NUMBER][reg_id[11]] = 0;

    /* DHD_RX_COMPLETE_0: thread 14 */
    local_regs[IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_1, dhd_rx_complete_wakeup_request);
    local_regs[IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER][reg_id[13]] = ((IMAGE_1_DHD_COMPLETE_COMMON_RADIO_DATA_ADDRESS + sizeof(RDD_DHD_COMPLETE_COMMON_RADIO_ENTRY_DTS)*0));
    local_regs[IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER][reg_id[12]] = 0;
    local_regs[IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER][reg_id[11]] = (BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_DHD_RX_COMPLETE_0 << 6));
    local_regs[IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER][reg_id[10]] = 0;
    local_regs[IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER][reg_id[9]] = IMAGE_1_PROCESSING_DHD_RX_COMPLETE_0_THREAD_NUMBER | (IMAGE_1_DHD_RX_COMPLETE_0_DISPATCHER_CREDIT_TABLE_ADDRESS << 16);

#endif
#endif

#if defined(RDP_SIM) || defined(XRDP_EMULATION) 
    /* copy the local registers initial values to the Context memory */
     MWRITE_BLK_32(sram_context, local_regs, sizeof(local_regs));
#else
     rdp_rnr_write_context(sram_context, local_regs, sizeof(local_regs));
#endif

}

static void image_2_context_set(uint32_t core_index, rdd_init_params_t *init_params)
{
    static uint32_t local_regs[NUM_OF_MAIN_RUNNER_THREADS][NUM_OF_LOCAL_REGS];
    uint32_t *sram_context;
    uint32_t task;


    sram_context = (uint32_t *)RUNNER_CORE_CONTEXT_ADDRESS(core_index);
#ifndef XRDP_EMULATION
    /* read the local registers from the Context memory - maybe it was initialized by the ACE compiler */
    MREAD_BLK_32(local_regs, sram_context, sizeof(local_regs));
#endif
    rdd_global_registers_init(core_index, local_regs, IMAGE_2_IMAGE_2_LAST);

    /* CPU_INTERRUPT_COALESCING: thread 0 */
    local_regs[IMAGE_2_IMAGE_2_INTERRUPT_COALESCING_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, interrupt_coalescing_1st_wakeup_request);
    local_regs[IMAGE_2_IMAGE_2_INTERRUPT_COALESCING_THREAD_NUMBER][reg_id[13]] = IMAGE_2_CPU_RECYCLE_INTERRUPT_COALESCING_TABLE_ADDRESS;
    local_regs[IMAGE_2_IMAGE_2_INTERRUPT_COALESCING_THREAD_NUMBER][reg_id[12]] = IMAGE_2_CPU_RECYCLE_RING_DESCRIPTOR_TABLE_ADDRESS |
        (IMAGE_2_CPU_RECYCLE_RING_INTERRUPT_COUNTER_TABLE_ADDRESS << 16);

    /* CPU_TX_TASK_0: thread 3 */
    local_regs[IMAGE_2_IMAGE_2_CPU_TX_0_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, cpu_tx_wakeup_request);
    local_regs[IMAGE_2_IMAGE_2_CPU_TX_0_THREAD_NUMBER][reg_id[10]] = IMAGE_2_IMAGE_2_CPU_TX_0_THREAD_NUMBER |(IMAGE_2_CPU_TX_SYNC_FIFO_TABLE_ADDRESS << 16);

    /* CPU_RECYCLE: thread 5 */
    local_regs[IMAGE_2_IMAGE_2_CPU_RECYCLE_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, cpu_recycle_wakeup_request);
    local_regs[IMAGE_2_IMAGE_2_CPU_RECYCLE_THREAD_NUMBER][reg_id[8]] = IMAGE_2_CPU_RECYCLE_SRAM_PD_FIFO_ADDRESS; 
    
    
	
#ifndef _CFE_
#ifdef CONFIG_DHD_RUNNER
    /* DHD_TIMER_THREAD_NUMBER: thread 6 */
    local_regs[IMAGE_2_IMAGE_2_DHD_TIMER_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, dhd_timer_1st_wakeup_request);
	
	/* DHD_TX_POST_0: thread 7 */
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_0_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, dhd_tx_post_wakeup_request);
    /* high = post_common_radio_ptr , low =radio_idx */
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_0_THREAD_NUMBER][reg_id[13]] = 0 | ((IMAGE_2_DHD_POST_COMMON_RADIO_DATA_ADDRESS + sizeof(RDD_DHD_POST_COMMON_RADIO_ENTRY_DTS)*0)) << 16;
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_0_THREAD_NUMBER][reg_id[12]] = 0;
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_0_THREAD_NUMBER][reg_id[11]] = IMAGE_2_DHD_TX_POST_PD_FIFO_TABLE_ADDRESS + sizeof(RDD_PROCESSING_TX_DESCRIPTOR_DTS)*0;

#endif /* CONFIG_DHD_RUNNER */
#endif /* !CFE */
	

    ag_drv_rnr_regs_cfg_ext_acc_cfg_set(core_index, PACKET_BUFFER_PD_PTR(IMAGE_2_DS_PACKET_BUFFER_ADDRESS, 0), 9, 8);
    /* PROCESSING : thread 8-13 */
    for (task = IMAGE_2_IMAGE_2_PROCESSING0_THREAD_NUMBER; task <= IMAGE_2_IMAGE_2_PROCESSING5_THREAD_NUMBER; task++) 
    {
        local_regs[task][reg_id[9]] = IMAGE_2_RX_FLOW_TABLE_ADDRESS << 16 | task;
        local_regs[task][reg_id[0]] = ADDRESS_OF(image_2, processing_wakeup_request) ;
        local_regs[task][reg_id[16]] = PACKET_BUFFER_PD_PTR(IMAGE_2_DS_PACKET_BUFFER_ADDRESS, (task-IMAGE_2_IMAGE_2_PROCESSING0_THREAD_NUMBER));
    }

#ifndef _CFE_
#ifdef CONFIG_DHD_RUNNER

    /* DHD_TX_POST_UPDATE_FIFO_READ: thread 14 */
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_UPDATE_FIFO_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, dhd_tx_post_update_fifo_wakeup_request);
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_UPDATE_FIFO_THREAD_NUMBER][reg_id[12]] = (((IMAGE_2_IMAGE_2_DHD_TX_POST_0_THREAD_NUMBER) << 4) + 9);
    local_regs[IMAGE_2_IMAGE_2_DHD_TX_POST_UPDATE_FIFO_THREAD_NUMBER][reg_id[11]] = IMAGE_2_DHD_TX_POST_UPDATE_FIFO_TABLE_ADDRESS;


    /* DHD_MCAST: thread 15 */
    local_regs[IMAGE_2_IMAGE_2_DHD_MCAST_THREAD_NUMBER][reg_id[0]] = ADDRESS_OF(image_2, dhd_mcast_wakeup_request);
    /* high = PD fifo bit ( = 0), low = DHD TX Post update fifo task number */
    local_regs[IMAGE_2_IMAGE_2_DHD_MCAST_THREAD_NUMBER][reg_id[12]] = (((IMAGE_2_IMAGE_2_DHD_TX_POST_UPDATE_FIFO_THREAD_NUMBER) << 4) + 9);
    local_regs[IMAGE_2_IMAGE_2_DHD_MCAST_THREAD_NUMBER][reg_id[11]] = IMAGE_2_DHD_MCAST_PD_FIFO_TABLE_ADDRESS | (IMAGE_2_DHD_MCAST_DISPATCHER_CREDIT_TABLE_ADDRESS << 16);
    local_regs[IMAGE_2_IMAGE_2_DHD_MCAST_THREAD_NUMBER][reg_id[10]] = (BB_ID_DISPATCHER_REORDER + (DISP_REOR_VIQ_DHD_MCAST << 6)) | (IMAGE_2_DHD_MCAST_UPDATE_FIFO_TABLE_ADDRESS << 16);

#endif /* CONFIG_DHD_RUNNER */
#endif /* !CFE */

#if defined(RDP_SIM) || defined(XRDP_EMULATION) 
    /* copy the local registers initial values to the Context memory */
     MWRITE_BLK_32(sram_context, local_regs, sizeof(local_regs));
#else
    rdp_rnr_write_context(sram_context, local_regs, sizeof(local_regs));
#endif
}


static void rdd_local_registers_init(rdd_init_params_t *init_params)
{
    uint32_t core_index;

    for (core_index = 0; core_index < GROUPED_EN_SEGMENTS_NUM; core_index++)
    {
        switch (rdp_core_to_image_map[core_index])
        {
        case image_0_runner_image:
            image_0_context_set(core_index, init_params);
            break;

        case image_1_runner_image:
            image_1_context_set(core_index, init_params);
            break;

        case image_2_runner_image:
            image_2_context_set(core_index, init_params);
            break;

        default: 
            bdmf_trace("ERROR driver %s:%u| unsupported Runner image = %d\n", __FILE__, __LINE__, rdp_core_to_image_map[core_index]);
            break;
        }
    }
}

static void rdd_layer2_header_copy_ptr_init(void)
{
    LAYER2_HEADER_COPY_ROUTINE_ARRAY(processing0_layer2_header_copy_routine_arr, image_1, processing_layer2_header_copy);
    LAYER2_HEADER_COPY_ROUTINE_ARRAY(processing1_layer2_header_copy_routine_arr, image_2, processing_layer2_header_copy);
    uint8_t i, core_index, *entry;
    RDD_BTRACE("\n");

    for (core_index = 0; core_index < GROUPED_EN_SEGMENTS_NUM; core_index++)
    {
        entry = (uint8_t *)RDD_LAYER2_HEADER_COPY_MAPPING_TABLE_PTR(core_index);
        if (rdp_core_to_image_map[core_index] == processing0_runner_image)
        {
            for (i = 0; i < RDD_LAYER2_HEADER_COPY_MAPPING_TABLE_SIZE; i++)
                RDD_LAYER2_HEADER_COPY_MAPPING_ENTRY_ROUTINE_WRITE(processing0_layer2_header_copy_routine_arr[i], (entry + (sizeof(RDD_LAYER2_HEADER_COPY_MAPPING_ENTRY_DTS) * i)));
        }
        else if (rdp_core_to_image_map[core_index] == processing1_runner_image)
        {
            for (i = 0; i < RDD_LAYER2_HEADER_COPY_MAPPING_TABLE_SIZE; i++)
                RDD_LAYER2_HEADER_COPY_MAPPING_ENTRY_ROUTINE_WRITE(processing1_layer2_header_copy_routine_arr[i], (entry + (sizeof(RDD_LAYER2_HEADER_COPY_MAPPING_ENTRY_DTS) * i)));
        }
    }
}

static int rdd_cpu_proj_init(void)
{
#ifndef _CFE_
    uint8_t def_idx = (uint8_t)BDMF_INDEX_UNASSIGNED;
#else
    uint8_t def_idx = 0;
#endif
    int rc = 0;

    rdd_cpu_tc_to_rqx_init(def_idx);
    rdd_cpu_vport_cpu_obj_init(def_idx);
    rdd_cpu_rx_meters_init();
#ifndef _CFE_
    rc = ag_drv_rnr_regs_cfg_cpu_wakeup_set(get_runner_idx(cpu_rx_runner_image),
        CPU_RX_METER_BUDGET_ALLOCATOR_THREAD_NUMBER);
#endif
    return rc;
}

int rdd_data_structures_init(rdd_init_params_t *init_params, RDD_HW_IPTV_CONFIGURATION_DTS *iptv_hw_config)
{
    bdmf_error_t rc = BDMF_ERR_OK;
#ifdef _CFE_
    int i;
#endif

    GROUP_MWRITE_32(RDD_ONE_VALUE_ADDRESS_ARR, 0, 1);

    rdd_local_registers_init(init_params);

    rdd_cpu_if_init();

    rdd_fpm_pool_number_mapping_cfg(iptv_hw_config->fpm_base_token_size);
#ifndef _CFE_
#ifdef CONFIG_DHD_RUNNER
    rdp_drv_dhd_skb_fifo_tbl_init();
#endif
    rdd_bridge_ports_init();
    if (!init_params->is_basic)
    {
		/* write global FPM congifuration */
        RDD_FPM_GLOBAL_CFG_FPM_BASE_LOW_WRITE_G(g_fpm_hw_cfg.fpm_base_low,RDD_FPM_GLOBAL_CFG_ADDRESS_ARR,0);
        RDD_FPM_GLOBAL_CFG_FPM_BASE_HIGH_WRITE_G(g_fpm_hw_cfg.fpm_base_high,RDD_FPM_GLOBAL_CFG_ADDRESS_ARR,0);
        RDD_FPM_GLOBAL_CFG_FPM_TOKEN_SIZE_ASR_8_WRITE_G(g_fpm_hw_cfg.fpm_token_size_asr_8,RDD_FPM_GLOBAL_CFG_ADDRESS_ARR,0);
#ifdef CONFIG_DHD_RUNNER       
        rdd_dhd_hw_cfg(&init_params->dhd_hw_config);
#endif
        rdd_iptv_processing_cfg(iptv_hw_config);
        rdd_qos_mapper_init();
    }
#endif

#if defined(DEBUG_PRINTS) && !defined(RDP_SIM)
        rdd_debug_prints_init();
#endif

    /* init first queue mapping */
    rdd_ag_us_tm_first_queue_mapping_set(drv_qm_get_us_start());
    rdd_ag_ds_tm_first_queue_mapping_set(drv_qm_get_ds_start());

    /* init bbh-queue */
    rdd_bbh_queue_init();

#ifndef _CFE_
    /* start flush task */
    rc = rc ? rc : rdd_scheduling_flush_timer_set();

    /* start budget allocation task */
    rc = rc ? rc : rdd_ds_budget_allocation_timer_set();

    rdd_rx_default_flow_init();
    
    rdd_max_pkt_len_table_init();

    rdd_ingress_qos_drop_miss_ratio_set(2);
#else
    /* enable all tx_flow (ports) */
    for (i=0; i <= 8; i++)
    {
        rdd_tx_flow_enable(i, rdpa_dir_ds, 1);
        rdd_tx_flow_enable(i, rdpa_dir_us, 1);
    }
#endif
    rdd_rx_flow_init();
    rdd_layer2_header_copy_mapping_init();
    rdd_layer2_header_copy_ptr_init();

    rdd_proj_init(init_params);
    rdd_actions_proj_init();
    rdd_tm_actions_proj_init();

    rdd_service_queues_actions_proj_init();
    rdd_service_queues_init(IMAGE_0_TM_BUDGET_ALLOCATOR_THREAD_NUMBER);

    rc = rc ? rc : rdd_cpu_proj_init();

    rc = rc ? rc : rdd_ag_natc_nat_cache_key0_mask_set(NATC_KEY0_DEF_MASK);

#ifdef USE_BDMF_SHELL
    /* register shell commands */
    rc = rc ? : rdd_make_shell_commands();
#endif
    return rc;
}

#ifndef _CFE_
static rdd_tcam_table_parm_t tcam_ic_flow_us_params =
{
    .module_id = TCAM_IC_MODULE_FLOW_US,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_flow_us_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
                      offsetof(RDD_RULE_BASED_CONTEXT_DTS, tcam_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_FLOW * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,   /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_flow_us_params
};

static rdd_tcam_table_parm_t tcam_ic_flow_ds_params =
{
    .module_id = TCAM_IC_MODULE_FLOW_DS,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_flow_ds_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
                      offsetof(RDD_RULE_BASED_CONTEXT_DTS, tcam_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_FLOW * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,   /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_flow_ds_params
};

static rdd_tcam_table_parm_t tcam_ic_qos_us_params =
{
    .module_id = TCAM_IC_MODULE_QOS_US,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_qos_us_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) + 
        offsetof(RDD_RULE_BASED_CONTEXT_DTS, tcam_qos_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_QOS * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_qos_us_params
};

static rdd_tcam_table_parm_t tcam_ic_qos_ds_params =
{
    .module_id = TCAM_IC_MODULE_QOS_DS,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_qos_ds_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_RULE_BASED_CONTEXT_DTS, tcam_qos_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_QOS * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_qos_ds_params
};
static rdd_tcam_table_parm_t tcam_ic_acl_ds_params =
{
    .module_id = TCAM_IC_MODULE_ACL_DS,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};
static rdd_module_t tcam_ic_acl_ds_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_RULE_BASED_CONTEXT_DTS, tcam_acl_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_ACL * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_acl_ds_params
};
static rdd_tcam_table_parm_t tcam_ic_acl_us_params =
{
    .module_id = TCAM_IC_MODULE_ACL_US,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};
static rdd_module_t tcam_ic_acl_us_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_RULE_BASED_CONTEXT_DTS, tcam_acl_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_ACL * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_acl_us_params
};

static rdd_tcam_table_parm_t tcam_ic_ip_flow_ds_params =
{
    .module_id = TCAM_IC_MODULE_IP_FLOW_DS,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_ip_flow_ds_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_FLOW_BASED_CONTEXT_DTS, tcam_ip_flow_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_IP_FLOW * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_ip_flow_ds_params
};

static rdd_tcam_table_parm_t tcam_ic_ip_flow_us_params =
{
    .module_id = TCAM_IC_MODULE_IP_FLOW_US,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_ip_flow_us_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_FLOW_BASED_CONTEXT_DTS, tcam_ip_flow_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_IP_FLOW * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_ip_flow_us_params
};

static rdd_tcam_table_parm_t tcam_ic_ip_flow_ds_miss_params =
{
    .module_id = TCAM_IC_MODULE_IP_FLOW_MISS_DS,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_ip_flow_ds_miss_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_FLOW_BASED_CONTEXT_DTS, tcam_ip_flow_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_IP_FLOW * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_ip_flow_ds_miss_params
};

static rdd_tcam_table_parm_t tcam_ic_ip_flow_us_miss_params =
{
    .module_id = TCAM_IC_MODULE_IP_FLOW_MISS_US,
    .scratch_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch) + TCAM_IC_SCRATCH_KEY_OFFSET,
};

static rdd_module_t tcam_ic_ip_flow_us_miss_module =
{
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_FLOW_BASED_CONTEXT_DTS, tcam_ip_flow_result),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TCAM_IC_IP_FLOW * 4)),
    .cfg_ptr = RDD_TCAM_IC_CFG_TABLE_ADDRESS_ARR,  /* Instance offset will be added at init time */
    .init = rdd_tcam_module_init,
    .params = &tcam_ic_ip_flow_us_miss_params
};


static rdd_module_t ingress_filter_module =
{
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_INGRESS_FILTERS * 4)),
    .cfg_ptr = RDD_INGRESS_FILTER_CFG_ADDRESS_ARR,
    .init = rdd_ingress_filter_module_init,
    .params = NULL
};

/* Bridge */

static rdd_bridge_module_param_t bridge_params_ds =
{
    .bridge_lkps_ready = 0,
    .aggregation_en = 0,
    .bridge_module_actions.hit = 1,
    .bridge_module_actions.bridge_fw_failed_action = 0,
    .bridge_module_actions.vlan_aggregation_action = 0,
    .module_id = BRIDGE_FLOW_DS
};

static rdd_bridge_module_param_t bridge_params_us =
{
    .bridge_lkps_ready = 1,
    .aggregation_en = 0,
    .bridge_module_actions.hit = 1,
    .bridge_module_actions.bridge_fw_failed_action = 0,
    .bridge_module_actions.vlan_aggregation_action = 0,
    .module_id = BRIDGE_FLOW_US
};

static rdd_module_t bridge_module_ds =
{
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_BRIDGE * 4)),
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) + 
        offsetof(RDD_RULE_BASED_CONTEXT_DTS, bridge_port_vector),
    .cfg_ptr = RDD_BRIDGE_CFG_TABLE_ADDRESS_ARR,
    .init = rdd_bridge_module_init,
    .params = &bridge_params_ds
};

static rdd_module_t bridge_module_us =
{
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_BRIDGE * 4)),
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) + 
        offsetof(RDD_RULE_BASED_CONTEXT_DTS, bridge_port_vector),
    .cfg_ptr = RDD_BRIDGE_CFG_TABLE_ADDRESS_ARR,
    .init = rdd_bridge_module_init,
    .params = &bridge_params_us
};

/* IPTV */
static iptv_params_t iptv_params =
{
    .key_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch),
    .hash_tbl_idx = 1
};

static rdd_module_t iptv_module =
{
    .init = rdd_iptv_module_init,
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_IPTV * 4)),
    .cfg_ptr = RDD_IPTV_CFG_TABLE_ADDRESS_ARR,
    .params = (void *)&iptv_params
};
#endif

/* IP CLASS */
static natc_params_t natc_params =
{
    .connection_key_offset = offsetof(RDD_PACKET_BUFFER_DTS, scratch),
};

static rdd_module_t ip_flow =
{
    .init = rdd_nat_cache_init,
    /* NTAC returns 60B (first 4B are control) */
    /* For 8B alignment, 4B are added */
    /* LD_CONTEXT macro adds 8B (control) */
    .context_offset = offsetof(RDD_PACKET_BUFFER_DTS, classification_contexts_list) +
        offsetof(RDD_FLOW_BASED_CONTEXT_DTS, flow_cache),
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_NAT_CACHE * 4)),
    .cfg_ptr = RDD_NAT_CACHE_CFG_ADDRESS_ARR,
    .params = (void *)&natc_params
};

/* Tunnels parser */
static tunnels_parsing_params_t tunnels_parsing_params =
{
    .tunneling_enable = 0
};

rdd_module_t tunnels_parsing =
{
    .init = rdd_tunnels_parsing_init,
    .cfg_ptr = RDD_TUNNELS_PARSING_CFG_ADDRESS_ARR,
    .res_offset = (offsetof(RDD_PACKET_BUFFER_DTS, classification_results) +
        (CLASSIFICATION_RESULT_INDEX_TUNNELS_PARSING * 4)),
    .params = (void *)&tunnels_parsing_params
};

static void rdd_proj_init(rdd_init_params_t *init_params)
{
    /* Classification modules initialization */
    _rdd_module_init(&tunnels_parsing);
    _rdd_module_init(&ip_flow);
    if (init_params->is_basic)
        return;
#ifndef _CFE_ 
    _rdd_module_init(&tcam_ic_flow_us_module);
    _rdd_module_init(&tcam_ic_qos_us_module);
    _rdd_module_init(&tcam_ic_flow_ds_module);
    _rdd_module_init(&tcam_ic_qos_ds_module);
    /* ACL exist only in none mode and use the same module ids of generic filter */
    if ( init_params->is_gateway == 0)
    {
        _rdd_module_init(&tcam_ic_acl_us_module);
        _rdd_module_init(&tcam_ic_acl_ds_module);
    }
    else
    {
    _rdd_module_init(&tcam_ic_ip_flow_ds_module);
    _rdd_module_init(&tcam_ic_ip_flow_us_module);
    }
    _rdd_module_init(&tcam_ic_ip_flow_us_miss_module);
    _rdd_module_init(&tcam_ic_ip_flow_ds_miss_module);
    _rdd_module_init(&ingress_filter_module);
    _rdd_module_init(&iptv_module);
    _rdd_module_init(&bridge_module_ds);
    _rdd_module_init(&bridge_module_us);
#endif
}

static void rdd_write_action(uint8_t core_index, uint16_t *action_arr, uint8_t size_of_array, uint8_t *ptr, uint8_t tbl_size, uint16_t exception_label)
{
    uint32_t action_index;
    for (action_index = 0; action_index < tbl_size; action_index++)
    {
        if (action_index < size_of_array)
            RDD_BYTES_2_BITS_WRITE(action_arr[action_index], ptr + (sizeof(action_arr[0]) * action_index));
        else
            RDD_BYTES_2_BITS_WRITE(exception_label, ptr + (sizeof(action_arr[0]) * action_index));
    }
}

static void rdd_tm_actions_proj_init(void)
{
    uint32_t action_index;
    RDD_BYTES_2_DTS *tm_action_ptr;
    uint16_t ds_actions_arr[] = {
        [0]  = ADDRESS_OF(image_0, basic_scheduler_update_dwrr),
        [1]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_queues),
        [2]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_basic_schedulers),
        [3]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_complex_schedulers),
        [4]  = ADDRESS_OF(image_0, basic_rate_limiter_queue_with_bs),
        [5]  = ADDRESS_OF(image_0, basic_rate_limiter_queue_with_cs_bs),
        [6]  = ADDRESS_OF(image_0, complex_rate_limiter_queue_sir),
        [7]  = ADDRESS_OF(image_0, complex_rate_limiter_queue_pir),
        [8]  = ADDRESS_OF(image_0, basic_rate_limiter_basic_scheduler_no_cs),
        [9]  = ADDRESS_OF(image_0, complex_rate_limiter_basic_scheduler_sir),
        [10] = ADDRESS_OF(image_0, complex_rate_limiter_basic_scheduler_pir),
        [11] = ADDRESS_OF(image_0, basic_rate_limiter_complex_scheduler),
        [12 ... 15] = ADDRESS_OF(image_0, ds_tx_scheduling_action_not_valid),
        [16] = ADDRESS_OF(image_0, ds_tx_scheduling_update_status)
    };
    uint16_t us_actions_arr[] = {
        [0]  = ADDRESS_OF(image_0, basic_scheduler_update_dwrr),
        [1]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_queues),
        [2]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_basic_schedulers),
        [3]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_complex_schedulers),
        [4]  = ADDRESS_OF(image_0, basic_rate_limiter_queue_with_bs),
        [5]  = ADDRESS_OF(image_0, basic_rate_limiter_queue_with_cs_bs),
        [6]  = ADDRESS_OF(image_0, complex_rate_limiter_queue_sir),
        [7]  = ADDRESS_OF(image_0, complex_rate_limiter_queue_pir),
        [8]  = ADDRESS_OF(image_0, basic_rate_limiter_basic_scheduler_no_cs),
        [9]  = ADDRESS_OF(image_0, complex_rate_limiter_basic_scheduler_sir),
        [10] = ADDRESS_OF(image_0, complex_rate_limiter_basic_scheduler_pir),
        [11] = ADDRESS_OF(image_0, basic_rate_limiter_complex_scheduler),
        [12] = ADDRESS_OF(image_0, ovl_rate_limiter),
        [13 ... 15] = ADDRESS_OF(image_0, scheduling_action_not_valid),
        [16] = ADDRESS_OF(image_0, scheduling_update_status)
    };

    for (action_index = 0; action_index < RDD_TM_ACTION_PTR_TABLE_SIZE; action_index++)
    {
        tm_action_ptr = ((RDD_BYTES_2_DTS *)RDD_US_TM_TM_ACTION_PTR_TABLE_PTR(get_runner_idx(us_tm_runner_image))) + action_index;
        RDD_BYTES_2_BITS_WRITE(us_actions_arr[action_index], tm_action_ptr);

        tm_action_ptr = ((RDD_BYTES_2_DTS *)RDD_DS_TM_TM_ACTION_PTR_TABLE_PTR(get_runner_idx(ds_tm_runner_image))) + action_index;
        RDD_BYTES_2_BITS_WRITE(ds_actions_arr[action_index], tm_action_ptr);
    }
}

static void rdd_actions_proj_init(void)
{
    uint8_t core_index;

    uint16_t processing0_vlan_actions_arr[] = {
        [0] = ADDRESS_OF(image_1, gpe_vlan_action_cmd_drop),
        [1] = ADDRESS_OF(image_1, gpe_vlan_action_cmd_dscp),
        [2] = ADDRESS_OF(image_1, gpe_vlan_action_cmd_mac_hdr_copy),                
        [3] = ADDRESS_OF(image_1, gpe_cmd_replace_16),
        [4] = ADDRESS_OF(image_1, gpe_cmd_replace_32),
        [5] = ADDRESS_OF(image_1, gpe_cmd_replace_bits_16),                
        [6] = ADDRESS_OF(image_1, gpe_cmd_copy_bits_16),
        [7 ... 16] = ADDRESS_OF(image_1, gpe_cmd_skip_if),
    };

    uint16_t processing1_vlan_actions_arr[] = {
        [0] = ADDRESS_OF(image_2, gpe_vlan_action_cmd_drop),
        [1] = ADDRESS_OF(image_2, gpe_vlan_action_cmd_dscp),
        [2] = ADDRESS_OF(image_2, gpe_vlan_action_cmd_mac_hdr_copy),                
        [3] = ADDRESS_OF(image_2, gpe_cmd_replace_16),
        [4] = ADDRESS_OF(image_2, gpe_cmd_replace_32),
        [5] = ADDRESS_OF(image_2, gpe_cmd_replace_bits_16),                
        [6] = ADDRESS_OF(image_2, gpe_cmd_copy_bits_16),
        [7 ... 16] = ADDRESS_OF(image_2, gpe_cmd_skip_if),
    };

    for (core_index = 0; core_index < GROUPED_EN_SEGMENTS_NUM; core_index++)
    {
        /* setting group 0 - processing */
        if (rdp_core_to_image_map[core_index] == processing0_runner_image)
        {
             rdd_write_action(core_index, processing0_vlan_actions_arr, sizeof(processing0_vlan_actions_arr)/sizeof(processing0_vlan_actions_arr[0]),
                (uint8_t *)RDD_VLAN_ACTION_GPE_HANDLER_PTR_TABLE_PTR(core_index), RDD_VLAN_ACTION_GPE_HANDLER_PTR_TABLE_SIZE, image_1_action_exception);
        }
        else if (rdp_core_to_image_map[core_index] == processing1_runner_image)
        {
            rdd_write_action(core_index, processing1_vlan_actions_arr, sizeof(processing1_vlan_actions_arr)/sizeof(processing1_vlan_actions_arr[0]),
                (uint8_t *)RDD_VLAN_ACTION_GPE_HANDLER_PTR_TABLE_PTR(core_index), RDD_VLAN_ACTION_GPE_HANDLER_PTR_TABLE_SIZE, image_2_action_exception);
        }
    }
}

static void rdd_service_queues_actions_proj_init(void)
{
    uint32_t action_index;
    uint16_t sq_actions_arr[] = {
        [0]  = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [1]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_queues),
        [2]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_basic_schedulers),
        [3]  = ADDRESS_OF(image_0, complex_scheduler_update_dwrr_complex_schedulers),
        [4]  = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [5]  = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [6]  = ADDRESS_OF(image_0, complex_rate_limiter_queue_sir),
        [7]  = ADDRESS_OF(image_0, complex_rate_limiter_queue_pir),
        [8]  = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [9]  = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [10] = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [11] = ADDRESS_OF(image_0, basic_rate_limiter_complex_scheduler),
        [12] = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [13 ... 15] = ADDRESS_OF(image_0, scheduling_service_queues_action_not_valid),
        [16] = ADDRESS_OF(image_0, scheduling_service_queues_update_status)
    };

    for (action_index = 0; action_index < RDD_TM_ACTION_PTR_TABLE_SIZE; action_index++)
    {
        RDD_BYTES_2_BITS_WRITE_G(sq_actions_arr[action_index], RDD_SERVICE_QUEUES_TM_ACTION_PTR_TABLE_ADDRESS_ARR, action_index);
    }
}

