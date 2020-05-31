/*****************************************************************************
 *   Copyright(C)2009-2019 by VSF Team                                       *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 ****************************************************************************/

/*============================ INCLUDES ======================================*/
#include "osa_service/vsf_osa_service_cfg.h"

#if VSF_USE_MSG_TREE == ENABLED

#define __VSF_MSG_TREE_CLASS_IMPLEMENT
#include "vsf_msg_tree.h"
/*============================ MACROS ========================================*/
/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/

implement_vsf_rng_buf(__bfs_node_fifo_t, uint16_t, NO_RNG_BUF_PROTECT)

void vsf_msgt_init(vsf_msgt_t* ptObj, const vsf_msgt_cfg_t* ptCFG)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj && NULL != ptCFG);
    VSF_OSA_SERVICE_ASSERT(     (NULL != ptCFG->ptInterfaces) 
                            ||  (0 != ptCFG->chTypeNumbers));
    
    this.NodeTypes = *ptCFG;
}

const vsf_msgt_node_t* vsf_msgt_get_next_node_within_container(const vsf_msgt_node_t* ptNode)
{
    do {
        if (NULL == ptNode) {
            break;
        }
        //! check offset of next node
        if (0 == ptNode->Offset.iNext) {
            //!< the last node
            ptNode = NULL;
            break;
        }
        //! point to next node with offset 
        ptNode = (vsf_msgt_node_t*)((intptr_t)ptNode + (intptr_t)(ptNode->Offset.iNext));
    } while(0);
    return ptNode;
}

static bool __msgt_check_status(vsf_msgt_t* ptObj,
                                const vsf_msgt_node_t* ptNode,
                                uint_fast8_t chStatusMask )
{
    vsf_msgt_method_status_t* fnStatus = NULL;
    uint_fast8_t chID = ptNode->chID;
    class_internal(ptObj, ptThis, vsf_msgt_t);

    fnStatus = this.NodeTypes.ptInterfaces[chID].Status;

    if (NULL != fnStatus) {
        //!< get status
        vsf_msgt_node_status_t tStatus = (*fnStatus)((vsf_msgt_node_t*)ptNode);
        if ((tStatus & chStatusMask) != chStatusMask) {
            return false;
        }
    }

    return true;
}
/*----------------------------------------------------------------------------*
 * Node shooting algorithm                                                    *
 *----------------------------------------------------------------------------*/

static bool __msgt_shoot(   vsf_msgt_t* ptObj,
                            const vsf_msgt_node_t *ptNode,
                            uintptr_t pBulletInfo)
{
    uint_fast8_t chID = ptNode->chID;
    bool bResult = false;
    vsf_msgt_method_shoot_t* fnShoot = NULL;
    class_internal(ptObj, ptThis, vsf_msgt_t);

    fnShoot = this.NodeTypes.ptInterfaces[chID].Shoot;

    if (NULL != fnShoot) {
        bResult = fnShoot(ptNode, pBulletInfo);
    }

    return bResult;
}

const vsf_msgt_node_t * vsf_msgt_shoot_top_node(  vsf_msgt_t* ptObj,
                                            const vsf_msgt_node_t *ptRoot,
                                            uintptr_t pBulletInfo)
{
    const vsf_msgt_node_t* ptItem = NULL;
    class_internal(ptObj, ptThis, vsf_msgt_t);

    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);

    do {
        if (NULL == ptRoot) {
            break;
        }

        if (!__msgt_check_status(   ptObj, 
                                    ptRoot,
                                    VSF_MSGT_NODE_VALID)) {
            break;
        }

        //! shoot the root item
        if (__msgt_shoot(ptObj, ptRoot, pBulletInfo)) {
            ptItem = (vsf_msgt_node_t *)ptRoot;
        } else if (ptRoot->tAttribute._.bIsContainer){
            if (!ptRoot->tAttribute._.bIsTransparent) {
                //! not in the range
                break;
            }
            //! transparent container
        } else {
            //! not in the range
            break;
        }

        //! check nodes in this container
        if (!__msgt_check_status(   ptObj, 
                                    ptItem,
                                    VSF_MSGT_NODE_HIDE_CONTENT)) {
            vsf_msgt_container_t* ptContainer = (vsf_msgt_container_t*)ptItem;
            const vsf_msgt_node_t * ptNode = ptContainer->ptNode;

            while (NULL != ptNode) {
                if (__msgt_shoot(ptObj, ptNode, pBulletInfo)) {
                    //! shoot a node
                    ptItem = ptNode;
                    break;
                }

                ptNode = vsf_msgt_get_next_node_within_container(ptNode);
            }

        } while (0);

    } while(0);
    return ptItem;
}


const vsf_msgt_node_t * vsf_msgt_shoot_node(  vsf_msgt_t* ptObj,
                                        const vsf_msgt_node_t *ptRoot,
                                        uintptr_t pBulletInfo)
{
    const vsf_msgt_node_t* ptItem = NULL;
    class_internal(ptObj, ptThis, vsf_msgt_t);

    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);

    
    do {
        const vsf_msgt_node_t* ptNode = ptRoot;

        do {
            ptNode = vsf_msgt_shoot_top_node(ptObj, ptNode, pBulletInfo);
            if (NULL == ptNode) {
                break;
            }
            
            if (ptItem == ptNode) {
                //! the same container (we only hit a container)
                break;
            }
            ptItem = ptNode;
            if (!ptItem->tAttribute._.bIsContainer) {
                //!< the leaf node
                break;
            } /* else; find a container, check the contained nodes*/
            
        } while(true);

    } while (0);

    return ptItem;
}

/*----------------------------------------------------------------------------*
 * Message Handing                                                            *
 *----------------------------------------------------------------------------*/

static void __vsf_msg_handling_init(   __vsf_msgt_msg_handling_fsm_t* ptFSM,
                                vsf_msgt_t* ptObj,
                                vsf_msgt_msg_t* ptMessage,
                                uint_fast8_t chStatusMask)
{
    ptFSM->chState = 0;
    ptFSM->ptMessage = ptMessage;
    ptFSM->chStatusMask = chStatusMask;
}

#undef THIS_FSM_STATE
#define THIS_FSM_STATE  ptFSM->chState

#define RESET_MSGT_HANDLING_FSM()         \
        do { THIS_FSM_STATE = 0; } while(0)


fsm_rt_t __vsf_msg_handling(__vsf_msgt_msg_handling_fsm_t *ptFSM, 
                            vsf_msgt_t* ptObj,
                            const vsf_msgt_node_t* ptNode)
{
    enum {
        START = 0,
        CHECK_STATUS,
        GET_HANDLER,
        HANDLE_FSM,
#if VSF_KERNEL_CFG_EDA_SUPPORT_SUB_CALL == ENABLED
        HANDLE_SUBCALL,
#endif
    };
    class_internal(ptObj, ptThis, vsf_msgt_t);

    switch (THIS_FSM_STATE) {
        case START:
            if (0 == ptFSM->chStatusMask) {
                THIS_FSM_STATE = GET_HANDLER;
                break;
            } else {
                THIS_FSM_STATE ++;
            }
            //break;
        case CHECK_STATUS:
            if (ptNode->chID >= this.NodeTypes.chTypeNumbers) {
                //! illegal Node ID detected, ignore this
                RESET_MSGT_HANDLING_FSM();
                return fsm_rt_err;
            } else {
                if (!__msgt_check_status(   ptObj, 
                                            ptNode, 
                                            //(VSF_MSGT_NODE_VALID | VSF_MSGT_NODE_ENABLED)
                                            ptFSM->chStatusMask
                                            )) {
                    //! not valid or enabled
                    RESET_MSGT_HANDLING_FSM();
                    return fsm_rt_err;
                }
                THIS_FSM_STATE = GET_HANDLER;
            }
            //break;
        case GET_HANDLER: {
            const vsf_msgt_handler_t* ptHandler = NULL;
            uint_fast8_t chID = ptNode->chID;

            ptHandler = &this.NodeTypes.ptInterfaces[chID].tMessageHandler;

            if (NULL == ptHandler->fn.fnFSM) {
                //! empty handler
                RESET_MSGT_HANDLING_FSM();
                return fsm_rt_err;
            }
            ptFSM->ptHandler = ptHandler;

            switch (ptHandler->u2Type) {
                case VSF_MSGT_NODE_HANDLER_TYPE_FSM:
                    THIS_FSM_STATE = HANDLE_FSM;
                    break;
    #if VSF_KERNEL_CFG_EDA_SUPPORT_SUB_CALL == ENABLED
                case VSF_MSGT_NODE_HANDLER_TYPE_SUBCALL:
                    if (NULL == vsf_eda_get_cur()) {
                        //! it's not possible to call sub eda
                        RESET_MSGT_HANDLING_FSM();
                        return (fsm_rt_t)VSF_ERR_NOT_SUPPORT;
                    }
                    THIS_FSM_STATE = HANDLE_SUBCALL;
                    break;
    #endif
                case VSF_MSGT_NODE_HANDLER_TYPE_EDA: {
                    vsf_err_t tErr = vsf_eda_post_msg(ptHandler->fn.ptEDA, ptFSM->ptMessage);
                    VSF_OSA_SERVICE_ASSERT(tErr == VSF_ERR_NONE);
                    UNUSED_PARAM(tErr);
                    RESET_MSGT_HANDLING_FSM();
                    return fsm_rt_cpl;
                }
                default:
                    //!< unknown type
                    RESET_MSGT_HANDLING_FSM();
                    return fsm_rt_err;
            }
            break;
        }

        case HANDLE_FSM: {
            fsm_rt_t tfsm = ptFSM->ptHandler->fn.fnFSM((vsf_msgt_node_t *)ptNode, ptFSM->ptMessage);
            if (fsm_rt_cpl == tfsm) {
                //! message has been handled
                RESET_MSGT_HANDLING_FSM();
                return fsm_rt_cpl;
            } else if (tfsm < 0) {
                //! message is not handled
                RESET_MSGT_HANDLING_FSM();
                return fsm_rt_err;
            } else {
                return tfsm;
            }
            break;
        }
        
#if VSF_KERNEL_CFG_EDA_SUPPORT_SUB_CALL == ENABLED
        case HANDLE_SUBCALL:
            //! todo add support for subcall
            RESET_MSGT_HANDLING_FSM();
            return fsm_rt_err;
#endif
    }

    return fsm_rt_on_going;
}


/*----------------------------------------------------------------------------*
 * Backward Message Propagation                                               *
 *----------------------------------------------------------------------------*/
#undef THIS_FSM_STATE
#define THIS_FSM_STATE  this.BW.chState

#define RESET_MSGT_BACKWARD_PROPAGATE_MSG_FSM()                                 \
        do { THIS_FSM_STATE = 0; } while(0)

fsm_rt_t vsf_msgt_backward_propagate_msg(   vsf_msgt_t* ptObj,
                                            const vsf_msgt_node_t *ptNode,
                                            vsf_msgt_msg_t *ptMessage)
{
    enum {
        START = 0,
        MSG_HANDLING,
        GET_PARENT,
    };
    fsm_rt_t tfsm = fsm_rt_on_going;
    class_internal(ptObj, ptThis, vsf_msgt_t);
    

    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    VSF_OSA_SERVICE_ASSERT(NULL != ptMessage);

    switch (THIS_FSM_STATE) {
        case START:
            if (NULL == ptNode) {
                return (fsm_rt_t)VSF_ERR_INVALID_PTR;
            } 
            this.BW.tMSGHandling.ptNode = ptNode;
            if (this.NodeTypes.chTypeNumbers > 0 && NULL == this.NodeTypes.ptInterfaces) {
                return (fsm_rt_t)VSF_ERR_INVALID_PARAMETER;
            }
            __vsf_msg_handling_init(&this.BW.tMSGHandling, 
                                    ptObj, 
                                    ptMessage, 
                                    (VSF_MSGT_NODE_VALID | VSF_MSGT_NODE_ENABLED));
            THIS_FSM_STATE++;
            //break;

        case MSG_HANDLING:
            tfsm = __vsf_msg_handling(&this.BW.tMSGHandling, ptObj, this.BW.tMSGHandling.ptNode);
            if (tfsm < 0) {
                THIS_FSM_STATE = GET_PARENT;
                break;
            } else if (fsm_rt_cpl == tfsm) {
                RESET_MSGT_BACKWARD_PROPAGATE_MSG_FSM();
                //return fsm_rt_cpl;
            } else if (tfsm >= fsm_rt_user) {
                RESET_MSGT_BACKWARD_PROPAGATE_MSG_FSM();
            }
            return tfsm;

        case GET_PARENT:
            //! backward propagate
            if (NULL == this.BW.tMSGHandling.ptNode->ptParent) {
                //! no parent
                RESET_MSGT_BACKWARD_PROPAGATE_MSG_FSM();
                return (fsm_rt_t)VSF_MSGT_ERR_MSG_NOT_HANDLED;
            }
            this.BW.tMSGHandling.ptNode = 
                (const vsf_msgt_node_t *)this.BW.tMSGHandling.ptNode->ptParent;
            THIS_FSM_STATE = MSG_HANDLING;
            break;
    }
    

    return fsm_rt_on_going;
}

const vsf_msgt_node_t *vsf_msgt_backward_propagate_msg_get_last_node(
                                                            vsf_msgt_t* ptObj)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    
    return this.BW.tMSGHandling.ptNode;
}

/*----------------------------------------------------------------------------*
 * Forward Message Propagation using Pre-Order Travesal Algorithm             *
 *----------------------------------------------------------------------------*/
SECTION(".text.vsf.osa_service.msg_tree"
        ".vsf_msgt_forward_propagate_msg_pre_order_traversal_init")
void vsf_msgt_forward_propagate_msg_pre_order_traversal_init(vsf_msgt_t* ptObj,
                                             bool bSupportContainerPostHandling)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    
    this.FWPOT.chState = 0;
    this.FWPOT.bSupportContainerPostHandling = bSupportContainerPostHandling;
}

SECTION(".text.vsf.osa_service.msg_tree"
        ".vsf_msgt_forward_propagate_msg_pre_order_traversal_setting")
void vsf_msgt_forward_propagate_msg_pre_order_traversal_setting(vsf_msgt_t* ptObj, 
                                                bool bSupportContainerPostHandling)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    this.FWPOT.bSupportContainerPostHandling = bSupportContainerPostHandling;
}

#undef THIS_FSM_STATE
#define THIS_FSM_STATE  this.FWPOT.chState

#define RESET_MSGT_FW_POT_PROPAGATE_MSG_FSM()                                   \
        do { THIS_FSM_STATE = 0; } while(0)

SECTION(".text.vsf.osa_service.msg_tree"
        ".vsf_msgt_forward_propagate_msg_pre_order_traversal")
fsm_rt_t vsf_msgt_forward_propagate_msg_pre_order_traversal(
                                            vsf_msgt_t* ptObj,
                                            const vsf_msgt_node_t* ptRoot,
                                            vsf_msgt_msg_t* ptMessage,
                                            uint_fast8_t chStatusMask)
{
    enum {
        START = 0,
        VISIT_ITEM,
        FETCH_NEXT_ITEM,
    };

    fsm_rt_t tfsm = fsm_rt_on_going;
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    VSF_OSA_SERVICE_ASSERT(NULL != ptMessage);
    

    switch (THIS_FSM_STATE) {
        case START:
            if (NULL == ptRoot) {
                return (fsm_rt_t)VSF_ERR_INVALID_PTR;
            }
            if (    (this.NodeTypes.chTypeNumbers > 0) 
                &&  (NULL == this.NodeTypes.ptInterfaces)) {
                return (fsm_rt_t)VSF_ERR_INVALID_PARAMETER;
            }
            this.FWPOT.tMSGHandling.ptNode = ptRoot;
            ((vsf_msgt_node_t* )ptRoot)->tAttribute._.bVisited = false;
            __vsf_msg_handling_init(&this.FWPOT.tMSGHandling, 
                                    ptObj, 
                                    ptMessage, 
                                    chStatusMask);

            THIS_FSM_STATE++;
            // break;

        case VISIT_ITEM:
            tfsm = __vsf_msg_handling(  &this.FWPOT.tMSGHandling, 
                                        ptObj, 
                                        this.FWPOT.tMSGHandling.ptNode);
            if (    tfsm < 0 
                ||  fsm_rt_cpl == tfsm) {

                THIS_FSM_STATE = FETCH_NEXT_ITEM;                
                // fall through
            } else {
                return tfsm;
            }

        case FETCH_NEXT_ITEM: {
            const vsf_msgt_node_t* ptItem = this.FWPOT.tMSGHandling.ptNode;
            const vsf_msgt_node_t* ptTemp = NULL;
            const vsf_msgt_container_t* ptContainer = 
                (const vsf_msgt_container_t *)ptItem;

            if (false == (((vsf_msgt_node_t* )ptItem)->tAttribute._.bVisited)) { 
                //! a normal node is visited
                ((vsf_msgt_node_t* )ptItem)->tAttribute._.bVisited = true;
                if (ptItem->tAttribute._.bIsContainer && tfsm == fsm_rt_cpl) {
                    ptTemp = ptContainer->ptNode;
                } else if (ptItem == ptRoot) {
                    RESET_MSGT_FW_POT_PROPAGATE_MSG_FSM();
                    return fsm_rt_cpl;
                }
            } else if (ptItem == ptRoot) {
                RESET_MSGT_FW_POT_PROPAGATE_MSG_FSM();
                return fsm_rt_cpl;
            }

            while(NULL == ptTemp) {
                ptTemp = vsf_msgt_get_next_node_within_container(ptItem);
                if (NULL != ptTemp) {
                    break;
                } 
                //! it is the last item in the container, return to previous level
                ptItem = (const vsf_msgt_node_t*)ptItem->ptParent;

                if (NULL == ptItem) {
                    //! it is the top container / root container, cpl
                    RESET_MSGT_FW_POT_PROPAGATE_MSG_FSM();
                    return fsm_rt_cpl;
                }  

                if (this.FWPOT.bSupportContainerPostHandling) {
                    //! container post handling
                    break;
                }
            }

            if (NULL != ptTemp) {
                this.FWPOT.tMSGHandling.ptNode = (const vsf_msgt_node_t *)ptTemp;
                ((vsf_msgt_node_t* )ptTemp)->tAttribute._.bVisited = false;
            } else {
                //! container post handling
                this.FWPOT.tMSGHandling.ptNode = (const vsf_msgt_node_t *)ptItem;
            }

            THIS_FSM_STATE = VISIT_ITEM;

            break;
        }
    }

    return fsm_rt_on_going;
}

/*----------------------------------------------------------------------------*
 * Forward Message Propagation using DFS algorithm                            *
 *----------------------------------------------------------------------------*/

SECTION(".text.vsf.osa_service.msg_tree.vsf_msgt_forward_propagate_msg_dfs")
void vsf_msgt_forward_propagate_msg_dfs_init(vsf_msgt_t* ptObj)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);

    this.FWDFS.chState = 0;
}

#undef THIS_FSM_STATE
#define THIS_FSM_STATE  this.FWDFS.chState

#define RESET_MSGT_FW_DFS_PROPAGATE_MSG_FSM()                                   \
        do { THIS_FSM_STATE = 0; } while(0)

SECTION(".text.vsf.osa_service.msg_tree.vsf_msgt_forward_propagate_msg_dfs")
fsm_rt_t vsf_msgt_forward_propagate_msg_dfs(vsf_msgt_t* ptObj,
                                            const vsf_msgt_node_t* ptRoot,
                                            vsf_msgt_msg_t* ptMessage)
{
    enum {
        START = 0,
        FETCH_DEEPEST_ITEM,
        VISIT_ITEM,
        FETCH_NEXT_ITEM,
        VISIT_THE_ENTRY_NODE,
    };
    fsm_rt_t tfsm = fsm_rt_on_going;
    class_internal(ptObj, ptThis, vsf_msgt_t);

    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    VSF_OSA_SERVICE_ASSERT(NULL != ptMessage);

    switch (THIS_FSM_STATE) {
        case START:
            if (NULL == ptRoot) {
                return (fsm_rt_t)VSF_ERR_INVALID_PTR;
            }
            if (    (this.NodeTypes.chTypeNumbers > 0) 
                &&  (NULL == this.NodeTypes.ptInterfaces)) {
                return (fsm_rt_t)VSF_ERR_INVALID_PARAMETER;
            }
            this.FWDFS.tMSGHandling.ptNode = ptRoot;

            __vsf_msg_handling_init(&this.FWDFS.tMSGHandling, 
                                    ptObj, 
                                    ptMessage, 
                                    (VSF_MSGT_NODE_VALID | VSF_MSGT_NODE_ENABLED));

            THIS_FSM_STATE++;
            // break;

        case FETCH_DEEPEST_ITEM: {
            const vsf_msgt_container_t* ptItem = 
                (const vsf_msgt_container_t *)this.FWDFS.tMSGHandling.ptNode;

            do {
                if (!ptItem->use_as__vsf_msgt_node_t.tAttribute._.bIsContainer) {
                    break;
                } else if (NULL == ptItem->ptNode) {
                    //! this is an empty container
                    break;
                } else {
                    ptItem = (const vsf_msgt_container_t*)ptItem->ptNode;
                }
            } while(true);

            this.FWDFS.tMSGHandling.ptNode = (const vsf_msgt_node_t *)ptItem;
            THIS_FSM_STATE = VISIT_ITEM;
            //break;
        }

        case VISIT_ITEM:
            tfsm = __vsf_msg_handling(  &this.FWDFS.tMSGHandling, 
                                        ptObj, 
                                        this.FWDFS.tMSGHandling.ptNode);
            if (    tfsm < 0 
                ||  fsm_rt_cpl == tfsm) {
                THIS_FSM_STATE = FETCH_NEXT_ITEM;
                break;
            } 
            return tfsm;
            
            //break;
    
        case FETCH_NEXT_ITEM: {
                const vsf_msgt_node_t* ptItem = this.FWDFS.tMSGHandling.ptNode;
                const vsf_msgt_node_t* ptTemp = NULL;
                if (ptItem == ptRoot) {
                    //! visited the entry node
                    RESET_MSGT_FW_DFS_PROPAGATE_MSG_FSM();
                    return fsm_rt_cpl;
                }
                do {
                    ptTemp = vsf_msgt_get_next_node_within_container(ptItem);
                    if (NULL != ptTemp) {
                        ptItem = ptTemp;
                        THIS_FSM_STATE = FETCH_DEEPEST_ITEM;
                        break;
                    } 
                    //! it is the last item in the container
                    ptItem = (const vsf_msgt_node_t*)ptItem->ptParent;

                    if (NULL == ptItem) {
                        //! it is the top container
                        RESET_MSGT_FW_DFS_PROPAGATE_MSG_FSM();
                        return fsm_rt_cpl;
                    } /*else if (ptItem == ptRoot) {
                        //! it is the last item
                        THIS_FSM_STATE = VISIT_ITEM;
                        break;
                    } */
                    THIS_FSM_STATE = VISIT_ITEM;
                } while(0);

                this.FWDFS.tMSGHandling.ptNode = ptItem;
                break;
            }
    }

    return fsm_rt_on_going;
}

/*----------------------------------------------------------------------------*
 * Forward Message Propagation using BFS algorithm                            *
 *----------------------------------------------------------------------------*/

SECTION(".text.vsf.osa_service.msg_tree.vsf_msgt_forward_propagate_msg_bfs")
void vsf_msgt_forward_propagate_msg_bfs_init(   vsf_msgt_t* ptObj, 
                                                uint16_t *phwFIFOBuffer, 
                                                uint_fast16_t hwBuffSize,
                                                bool bSupportContainerPostHandling)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    VSF_OSA_SERVICE_ASSERT(NULL != phwFIFOBuffer);
    VSF_OSA_SERVICE_ASSERT(hwBuffSize > 2);

    this.FWBFS.chState = 0;
    this.FWBFS.bSupportContainerPostHandling = bSupportContainerPostHandling;
    vsf_rng_buf_prepare(__bfs_node_fifo_t, 
                        &(this.FWBFS.tFIFO), 
                        phwFIFOBuffer, 
                        hwBuffSize);
}

SECTION(".text.vsf.osa_service.msg_tree.vsf_msgt_forward_propagate_msg_bfs_setting")
void vsf_msgt_forward_propagate_msg_bfs_setting(vsf_msgt_t* ptObj, 
                                                bool bSupportContainerPostHandling)
{
    class_internal(ptObj, ptThis, vsf_msgt_t);
    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    this.FWBFS.bSupportContainerPostHandling = bSupportContainerPostHandling;
}

#undef THIS_FSM_STATE
#define THIS_FSM_STATE  this.FWBFS.chState

#define RESET_MSGT_FW_BFS_PROPAGATE_MSG_FSM()                                   \
        do { THIS_FSM_STATE = 0; } while(0)

SECTION(".text.vsf.osa_service.msg_tree.vsf_msgt_forward_propagate_msg_bfs")
fsm_rt_t vsf_msgt_forward_propagate_msg_bfs(vsf_msgt_t* ptObj,
                                            const vsf_msgt_node_t* ptNode,
                                            vsf_msgt_msg_t* ptMessage,
                                            uint_fast8_t chStatusMask)
{
    enum {
        START = 0,
        FETCH_ITEM,
        VISIT_ITEM,        
    };
    fsm_rt_t tfsm = fsm_rt_on_going;
    uint16_t hwOffset = 0;
    class_internal(ptObj, ptThis, vsf_msgt_t);

    VSF_OSA_SERVICE_ASSERT(NULL != ptObj);
    VSF_OSA_SERVICE_ASSERT(NULL != ptMessage);

    switch (THIS_FSM_STATE) {
        case START:
            if (NULL == ptNode) {
                return (fsm_rt_t)VSF_ERR_INVALID_PTR;
            }
            if (    (this.NodeTypes.chTypeNumbers > 0) 
                &&  (NULL == this.NodeTypes.ptInterfaces)) {
                return (fsm_rt_t)VSF_ERR_INVALID_PARAMETER;
            }
            VSF_OSA_SERVICE_ASSERT (NULL != this.FWBFS.tFIFO.ptBuffer);

            ((vsf_msgt_node_t* )ptNode)->tAttribute._.bVisited = false;

            if (!vsf_rng_buf_send_one(  __bfs_node_fifo_t, 
                                        &(this.FWBFS.tFIFO), 
                                        0)) {
                return (fsm_rt_t)VSF_ERR_PROVIDED_RESOURCE_NOT_SUFFICIENT;
            }

            __vsf_msg_handling_init(&this.FWBFS.tMSGHandling, 
                                    ptObj, 
                                    ptMessage, 
                                    chStatusMask);
            THIS_FSM_STATE ++;
            // break;

        case FETCH_ITEM:
            if (!vsf_rng_buf_get_one(   __bfs_node_fifo_t, 
                                        &(this.FWBFS.tFIFO), 
                                        &hwOffset)) {
                //! search complete
                RESET_MSGT_FW_BFS_PROPAGATE_MSG_FSM();
                return fsm_rt_cpl;
            }
            //! calculate target item address
            this.FWBFS.tMSGHandling.ptNode = 
                (vsf_msgt_node_t *)((uintptr_t)ptNode + hwOffset);
            THIS_FSM_STATE = VISIT_ITEM;
            //break;

        case VISIT_ITEM:
            tfsm = __vsf_msg_handling(  &this.FWBFS.tMSGHandling, 
                                        ptObj, 
                                        this.FWBFS.tMSGHandling.ptNode);
            if (    tfsm < 0 
                ||  fsm_rt_cpl == tfsm) {
                THIS_FSM_STATE = FETCH_ITEM;

                if (this.FWBFS.bSupportContainerPostHandling) {
                    //! check whether the node is visited or not
                    if (this.FWBFS.tMSGHandling.ptNode->tAttribute._.bVisited) {
                        break;
                    }

                    //! mark node as visited
                    ((vsf_msgt_node_t *)(this.FWBFS.tMSGHandling.ptNode))->tAttribute._.bVisited = true;
                }

                if (    this.FWBFS.tMSGHandling.ptNode->tAttribute._.bIsContainer 
                    &&  tfsm == fsm_rt_cpl) {
                    //! add nodes to fifo
                    vsf_msgt_container_t* ptContainer = 
                        (vsf_msgt_container_t*)this.FWBFS.tMSGHandling.ptNode;
                    vsf_msgt_node_t *ptTemp = 
                        (vsf_msgt_node_t *)ptContainer->ptNode;

                    //! put all nodes into fifo
                    while(NULL != ptTemp) {
                        hwOffset = 
                            (uint16_t)((uintptr_t)ptTemp 
                                     - (uintptr_t)ptNode /*this.FWBFS.tMSGHandling.ptNode*/);

                        //! mark child node as unvisited
                        ptTemp->tAttribute._.bVisited = false;
                        if (!vsf_rng_buf_send_one(  __bfs_node_fifo_t, 
                                                    &(this.FWBFS.tFIFO), 
                                                    hwOffset)) {
                            RESET_MSGT_FW_BFS_PROPAGATE_MSG_FSM();
                            return (fsm_rt_t)
                                VSF_ERR_PROVIDED_RESOURCE_NOT_SUFFICIENT;
                        }
                        ptTemp = (vsf_msgt_node_t *)vsf_msgt_get_next_node_within_container(ptTemp);
                    } 

                    if (this.FWBFS.bSupportContainerPostHandling) {
                    //! send the container node again
                        hwOffset = 
                            (uint16_t)((uintptr_t)this.FWBFS.tMSGHandling.ptNode 
                                     - (uintptr_t)ptNode /*this.FWBFS.tMSGHandling.ptNode*/);

                        if (!vsf_rng_buf_send_one(  __bfs_node_fifo_t, 
                                                    &(this.FWBFS.tFIFO), 
                                                    hwOffset)) {
                            RESET_MSGT_FW_BFS_PROPAGATE_MSG_FSM();
                            return (fsm_rt_t)
                                VSF_ERR_PROVIDED_RESOURCE_NOT_SUFFICIENT;
                        }
                    } 
                } 
                break;

            } else if (VSF_MSGT_ERR_REQUEST_VISIT_PARENT == tfsm) {
                THIS_FSM_STATE = FETCH_ITEM;

                //! visit parent is requrested
                const vsf_msgt_node_t* ptTemp = NULL;
                if (NULL != this.FWBFS.tMSGHandling.ptNode->ptParent) {
                    ptTemp = (const vsf_msgt_node_t *)
                                this.FWBFS.tMSGHandling.ptNode->ptParent;
                } else {
                    /* no parent, revisit itself again to maitain the same behaviour*/
                    ptTemp = this.FWBFS.tMSGHandling.ptNode;
                }

                hwOffset =
                    (uint16_t)((uintptr_t)ptTemp - (uintptr_t)ptNode);
                if (!vsf_rng_buf_send_one(__bfs_node_fifo_t,
                    &(this.FWBFS.tFIFO),
                    hwOffset)) {
                    RESET_MSGT_FW_BFS_PROPAGATE_MSG_FSM();
                    return (fsm_rt_t)
                        VSF_ERR_PROVIDED_RESOURCE_NOT_SUFFICIENT;
                }

                break;

            } else if (VSF_MSGT_ERR_REUQEST_VISIT_AGAIN == tfsm) {
                THIS_FSM_STATE = FETCH_ITEM;

                const vsf_msgt_node_t* ptTemp = this.FWBFS.tMSGHandling.ptNode;
                hwOffset =
                    (uint16_t)((uintptr_t)ptTemp - (uintptr_t)ptNode);
                if (!vsf_rng_buf_send_one(__bfs_node_fifo_t,
                    &(this.FWBFS.tFIFO),
                    hwOffset)) {
                    RESET_MSGT_FW_BFS_PROPAGATE_MSG_FSM();
                    return (fsm_rt_t)
                        VSF_ERR_PROVIDED_RESOURCE_NOT_SUFFICIENT;
                }
                break;
            }

            return tfsm;
    }

    return fsm_rt_on_going;
}

#endif
/* EOF */
