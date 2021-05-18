/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef COMMON_IDS_H_
#define COMMON_IDS_H_

#ifdef __cplusplus
extern "C" {
#endif

enum rpc_command {
	RPC_COMMAND_APP_BT_NUS_SEND = 0x01,
	RPC_COMMAND_NET_BT_NUS_RECEIVE_CB = 0x02,
	RPC_COMMAND_APP_BT_SMP_SEND = 0x03,
	RPC_COMMAND_NET_BT_SMP_RECEIVE_CB = 0x04,
	RPC_COMMAND_APP_BT_SMP_GET_MTU = 0x05,
/*  RPC_COMMAND_NET_BT_MTU_SIZE_CB = 0x05, */
};

enum rpc_event {
	RPC_EVENT_TX_APP_ASYNC = 0x01,
	RPC_EVENT_TX_APP_ASYNC_RESULT = 0x02,
};

#ifdef __cplusplus
}
#endif

#endif /* COMMON_IDS_H_ */
