/*
 *  Author : WangBoJing , email : 1989wangbojing@gmail.com
 * 
 *  Copyright Statement:
 *  --------------------
 *  This software is protected by Copyright and the information contained
 *  herein is confidential. The software may not be copied and the information
 *  contained herein may not be used or disclosed except with the written
 *  permission of Author. (C) 2016
 * 
 *
 
****       *****
  ***        *
  ***        *                         *               *
  * **       *                         *               *
  * **       *                         *               *
  *  **      *                        **              **
  *  **      *                       ***             ***
  *   **     *       ******       ***********     ***********    *****    *****
  *   **     *     **     **          **              **           **      **
  *    **    *    **       **         **              **           **      *
  *    **    *    **       **         **              **            *      *
  *     **   *    **       **         **              **            **     *
  *     **   *            ***         **              **             *    *
  *      **  *       ***** **         **              **             **   *
  *      **  *     ***     **         **              **             **   *
  *       ** *    **       **         **              **              *  *
  *       ** *   **        **         **              **              ** *
  *        ***   **        **         **              **               * *
  *        ***   **        **         **     *        **     *         **
  *         **   **        **  *      **     *        **     *         **
  *         **    **     ****  *       **   *          **   *          *
*****        *     ******   ***         ****            ****           *
                                                                       *
                                                                      *
                                                                  *****
                                                                  ****


 *
 */

#include <pthread.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>
#include <pthread.h>

#include "NattyProtoClient.h"
#include "NattyTimer.h"
#include "NattyUtils.h"
#include "NattyNetwork.h"

static void ntySetupHeartBeatThread(void* self);
static void ntySetupRecvProcThread(void *self);
static void ntySendLogin(void *self);
static void ntySendLogout(void *self);
static void ntySendTimeout(int len);
static void* ntyRecvProc(void *arg);


#if 1 //

typedef void* (*RECV_CALLBACK)(void *arg);

typedef enum {
	STATUS_NETWORK_LOGIN,
	STATUS_NETWORK_PROXYDATA,
	STATUS_NETWORK_LOGOUT,
} StatusNetwork;


typedef struct _NATTYPROTOCOL {
	const void *_;
	C_DEVID devid; //App Or Device Id
	C_DEVID fromId;
	U8 level;
	U8 recvBuffer[RECV_BUFFER_SIZE];
	U16 recvLen;
	PROXY_CALLBACK onProxyCallback; //just for java
	RECV_CALLBACK onRecvCallback; //recv
	PROXY_CALLBACK onProxyFailed; //send data failed
	PROXY_CALLBACK onProxySuccess; //send data success
	PROXY_CALLBACK onProxyDisconnect;
	PROXY_CALLBACK onProxyReconnect;
	PROXY_CALLBACK onBindResult;
	PROXY_CALLBACK onUnBindResult;
	U8 heartbeartRun;
	U8 p2pHeartbeatRun;
	U8 u8RecvExitFlag;
	U8 u8HeartbeatExistFlag;
	pthread_t heartbeatThread_id;
	pthread_t recvThread_id;
	struct sockaddr_in serveraddr;
} NattyProto;

typedef struct _NATTYPROTO_OPERA {
	size_t size;
	void* (*ctor)(void *_self, va_list *params);
	void* (*dtor)(void *_self);
	void* (*heartbeat)(void *_self); //start 
	void (*login)(void *_self); //argument is optional
	void (*logout)(void *_self); //argument is optional
	void (*proxyReq)(void *_self, C_DEVID toId, U8 *buf, int length);
	void (*proxyAck)(void *_self, C_DEVID friId, U32 ack);
	void (*bind)(void *_self, C_DEVID did);
	void (*unbind)(void *_self, C_DEVID did);
#if (NEY_PROTO_VERSION > 'B')
	int* (*p2pconnectReq)(void *_self, void* fTree, C_DEVID id); //for p2p
	int* (*p2pconnectAck)(void *_self, void* fTree, C_DEVID id); //for p2p
	void (*p2pdataReq)(void *_self, C_DEVID toId, U8 *buf, int length);
	void (*p2pdataAck)(void *_self, C_DEVID toId, U8 *buf, int length);
	void (*p2pheartbeatReq)(void* _self);
	void (*p2pheartbeatAck)(void *_self, C_DEVID toId, U8 *buf, int length);
#endif
} NattyProtoOpera;



#endif




void* ntyProtoClientCtor(void *_self, va_list *params) {
	NattyProto *proto = _self;
	struct hostent *server = NULL;

	proto->onRecvCallback = ntyRecvProc;
	proto->level = STATUS_NETWORK_LOGIN;
	proto->p2pHeartbeatRun = 0;
	proto->heartbeartRun = 0;
	proto->recvLen = 0;
	proto->devid = 0;

	proto->heartbeatThread_id = 0;
	proto->recvThread_id = 0;

	proto->u8HeartbeatExistFlag = 0;
	proto->u8RecvExitFlag = 0;

#if 1 //server addr init
#if 0 //android JNI don't support gethostbyname
	server = gethostbyname(SERVER_NAME);    
	if (server == NULL) {        
		ntylog("ERROR, no such host as %s\n", SERVER_NAME);  
		//exit(0);   
		bzero((char *) &proto->serveraddr, sizeof(proto->serveraddr)); 
		return proto;
	}
	bzero((char *) &proto->serveraddr, sizeof(proto->serveraddr));    
	proto->serveraddr.sin_family = AF_INET;    
	bcopy((char *)server->h_addr, (char *)&proto->serveraddr.sin_addr.s_addr, server->h_length);    
	proto->serveraddr.sin_port = htons(SERVER_PORT);
#else
	bzero((char *) &proto->serveraddr, sizeof(proto->serveraddr));    
	proto->serveraddr.sin_family = AF_INET;    
	proto->serveraddr.sin_addr.s_addr = inet_addr(SERVER_NAME);
	proto->serveraddr.sin_port = htons(SERVER_PORT);
#endif	
	

	ntyGenCrcTable();

#if 0 //set network callback
	void *pNetwork = ntyNetworkInstance();
	((Network*)pNetwork)->onDataLost = ntySendTimeout;
#endif

#endif

	return proto;
}

void* ntyProtoClientDtor(void *_self) {
	NattyProto *proto = _self;

	proto->onRecvCallback = NULL;
	proto->level = STATUS_NETWORK_LOGOUT;
	proto->p2pHeartbeatRun = 0;
	proto->heartbeartRun = 0;
	proto->recvLen = 0;

#if 1 //should send logout packet to server
#endif

	return proto;
}

/*
 * heartbeat Packet
 * VERSION					1			BYTE
 * MESSAGE TYPE				1			BYTE (req, ack)
 * TYPE					1			BYTE 
 * DEVID					8			BYTE
 * ACKNUM					4			BYTE (Network Module Set Value)
 * CRC 					4			BYTE (Network Module Set Value)
 * 
 * send to server addr
 */

void* ntyProtoClientHeartBeat(void *_self) {
	NattyProto *proto = _self;

	int len, n;	
	U8 buf[NTY_LOGIN_ACK_LENGTH] = {0};	

	ntydbg(" heartbeatThread running\n");
	if (proto->heartbeartRun == 1) {		
		proto->heartbeartRun = 1;		
		return NULL;	
	}	
	proto->heartbeartRun = 1;
	void *pNetwork = ntyNetworkInstance();

	while (1) {		
		bzero(buf, NTY_LOGIN_ACK_LENGTH);
		sleep(HEARTBEAT_TIMEOUT);	
		if (proto->u8HeartbeatExistFlag) break;
		if (proto->devid == 0) continue; //set devid
		
		buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;	
		buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
		buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_HEARTBEAT_REQ;		
		*(C_DEVID*)(&buf[NTY_PROTO_DEVID_IDX]) = proto->devid;
		
		len = NTY_PROTO_LOGIN_REQ_CRC_IDX+sizeof(U32);
		
		n = ntySendFrame(pNetwork, &proto->serveraddr, buf, len);
	}
}

/*
 * Login Packet
 * VERSION					1			BYTE
 * MESSAGE TYPE				1			BYTE (req, ack)
 * TYPE					1			BYTE 
 * DEVID					8			BYTE
 * ACKNUM					4			BYTE (Network Module Set Value)
 * CRC 					4			BYTE (Network Module Set Value)
 * 
 * send to server addr
 */
void ntyProtoClientLogin(void *_self) {
	NattyProto *proto = _self;
	int len, n;	
	U8 buf[RECV_BUFFER_SIZE] = {0};	

	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;	
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_LOGIN_REQ;
	*(C_DEVID*)(&buf[NTY_PROTO_LOGIN_REQ_DEVID_IDX]) = proto->devid;	
	
	len = NTY_PROTO_LOGIN_REQ_CRC_IDX+sizeof(U32);				

	ntydbg(" ntyProtoClientLogin %d\n", __LINE__);
	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, len);
}

void ntyProtoClientBind(void *_self, C_DEVID did) {
	NattyProto *proto = _self;
	int len, n;	

	U8 buf[NORMAL_BUFFER_SIZE] = {0};	

	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;	
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_BIND_REQ;

	*(C_DEVID*)(&buf[NTY_PROTO_BIND_APPID_IDX]) = proto->devid;
	*(C_DEVID*)(&buf[NTY_PROTO_BIND_DEVICEID_IDX]) = did;
	len = NTY_PROTO_BIND_CRC_IDX + sizeof(U32);

	ntydbg(" ntyProtoClientBind --> ");

	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, len);
}

void ntyProtoClientUnBind(void *_self, C_DEVID did) {
	NattyProto *proto = _self;
	int len, n;	

	U8 buf[NORMAL_BUFFER_SIZE] = {0};	

	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;	
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_UNBIND_REQ;

	*(C_DEVID*)(&buf[NTY_PROTO_UNBIND_APPID_IDX]) = proto->devid;
	*(C_DEVID*)(&buf[NTY_PROTO_UNBIND_DEVICEID_IDX]) = did;
	len = NTY_PROTO_UNBIND_CRC_IDX + sizeof(U32);

	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, len);
}



void ntyProtoClientLogout(void *_self) {
	NattyProto *proto = _self;
	int len, n;	
	U8 buf[RECV_BUFFER_SIZE] = {0};	

	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;	
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_LOGOUT_REQ;
#if 1
	//ntyU64ToU8Array(&buf[NTY_PROTO_DEVID_IDX], proto->devid);
	memcpy(&buf[NTY_PROTO_DEVID_IDX], &proto->devid, sizeof(C_DEVID));
#else
	*(C_DEVID*)(&buf[NTY_PROTO_DEVID_IDX]) = proto->devid;	
#endif
	len = NTY_PROTO_LOGIN_REQ_CRC_IDX+sizeof(U32);				

	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, len);
}

/*
 * Server Proxy Data Transport
 * VERSION					1			 BYTE
 * MESSAGE TYPE			 	1			 BYTE (req, ack)
 * TYPE				 	1			 BYTE 
 * DEVID					8			 BYTE (self devid)
 * ACKNUM					4			 BYTE (Network Module Set Value)
 * DEST DEVID				8			 BYTE (friend devid)
 * CONTENT COUNT				2			 BYTE 
 * CONTENT					*(CONTENT COUNT)	 BYTE 
 * CRC 				 	4			 BYTE (Network Module Set Value)
 * 
 * send to server addr, proxy to send one client
 * 
 */

void ntyProtoClientProxyReq(void *_self, C_DEVID toId, U8 *buf, int length) {
	int n = 0;
	NattyProto *proto = _self;

	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_DATAPACKET_REQ;

	LOG("ntyProtoClientProxyReq");
#if 1
	memcpy(&buf[NTY_PROTO_DATAPACKET_DEVID_IDX], &proto->devid, sizeof(C_DEVID));
	memcpy(&buf[NTY_PROTO_DATAPACKET_DEST_DEVID_IDX], &toId, sizeof(C_DEVID));
#else
	*(C_DEVID*)(&buf[NTY_PROTO_DATAPACKET_DEVID_IDX]) = (C_DEVID) proto->devid;
	*(C_DEVID*)(&buf[NTY_PROTO_DATAPACKET_DEST_DEVID_IDX]) = toId;
#endif
	
	*(U16*)(&buf[NTY_PROTO_DATAPACKET_CONTENT_COUNT_IDX]) = (U16)length;
	length += NTY_PROTO_DATAPACKET_CONTENT_IDX;
	length += sizeof(U32);

	LOG("ntyProtoClientProxyReq, length:%d", length);
	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, length);
	
}

void ntyProtoClientProxyAck(void *_self, C_DEVID toId, U32 ack) {
	int len, n;	
	NattyProto *proto = _self;
	U8 buf[RECV_BUFFER_SIZE] = {0}; 
	
	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_DATAPACKET_ACK;
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_ACK; 
#if 0
	*(C_DEVID*)(&buf[NTY_PROTO_DEVID_IDX]) = (C_DEVID) proto->devid;		
	*(U32*)(&buf[NTY_PROTO_ACKNUM_IDX]) = ack+1;
	*(C_DEVID*)(&buf[NTY_PROTO_DEST_DEVID_IDX]) = toId;
#else
	memcpy(&buf[NTY_PROTO_DEVID_IDX], &proto->devid, sizeof(C_DEVID));
	memcpy(&buf[NTY_PROTO_DEST_DEVID_IDX], &toId, sizeof(C_DEVID));
	ack = ack+1;
	memcpy(&buf[NTY_PROTO_ACKNUM_IDX], &ack, sizeof(int));
#endif
	len = NTY_PROTO_CRC_IDX+sizeof(U32);				

	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, len);
}


static const NattyProtoOpera ntyProtoOpera = {
	sizeof(NattyProto),
	ntyProtoClientCtor,
	ntyProtoClientDtor,
	ntyProtoClientHeartBeat,
	ntyProtoClientLogin,
	ntyProtoClientLogout,
	ntyProtoClientProxyReq,
	ntyProtoClientProxyAck,
	ntyProtoClientBind,
	ntyProtoClientUnBind,
#if (NEY_PROTO_VERSION > 'B')
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
#endif	
};

const void *pNattyProtoOpera = &ntyProtoOpera;

static void *pProtoOpera = NULL;

void *ntyProtoInstance(void) {
	if (pProtoOpera == NULL) {
		ntydbg("ntyProtoInstance\n");
		pProtoOpera = New(pNattyProtoOpera);
	}
	return pProtoOpera;
}

void ntyProtoRelease(void *self) {
	return Delete(self);
}

static void ntySetupHeartBeatThread(void* self) {
#if 1
	NattyProto *proto = self;
	NattyProtoOpera * const * protoOpera = self;
	int err;

	if (self && (*protoOpera) && (*protoOpera)->heartbeat) {
		if (proto->heartbeatThread_id != 0) {
			ntydbg(" heart beat thread is running \n");
			return ;
		}
		err = pthread_create(&proto->heartbeatThread_id, NULL, (*protoOpera)->heartbeat, self);				
		if (err != 0) { 				
			ntydbg(" can't create thread:%s\n", strerror(err)); 
			return ;			
		}
	}
#else
	NattyProto *proto = self;
	int err;
	if (self && proto && proto->heartbeat) {		
		err = pthread_create(&proto->heartbeatThread_id, NULL, proto->heartbeat, self);				
		if (err != 0) { 				
			ntydbg(" can't create thread:%s\n", strerror(err)); 
			exit(0);				
		}
	}
#endif
}

static void ntySetupRecvProcThread(void *self) {
	//NattyProtoOpera * const * protoOpera = self;
	NattyProto *proto = self;
	int err;
	pthread_t recvThread_id;

	if (self && proto && proto->onRecvCallback) {	
		if (proto->recvThread_id != 0) {
			ntydbg(" recv thread is running \n");
			return ;
		}
		
		err = pthread_create(&proto->recvThread_id, NULL, proto->onRecvCallback, self);				
		if (err != 0) { 				
			ntydbg(" can't create thread:%s\n", strerror(err)); 
			return ;	
		}
	}
}

static void ntySendLogin(void *self) {
	NattyProtoOpera * const * protoOpera = self;

	ntydbg(" ntySendLogin %d\n", __LINE__);
	if (self && (*protoOpera) && (*protoOpera)->login) {
		return (*protoOpera)->login(self);
	}
}

static void ntySendLogout(void *self) {
	NattyProtoOpera * const * protoOpera = self;

	if (self && (*protoOpera) && (*protoOpera)->logout) {
		return (*protoOpera)->logout(self);
	}
}

int ntySendDataPacket(C_DEVID toId, U8 *data, int length) {
	int n = 0;
	void *self = ntyProtoInstance();
	NattyProtoOpera * const *protoOpera = self;
	NattyProto *proto = self;
	
	U8 buf[NTY_PROXYDATA_PACKET_LENGTH] = {0};
	
#if 0
	buf[NEY_PROTO_VERSION_IDX] = NEY_PROTO_VERSION;
	buf[NTY_PROTO_MESSAGE_TYPE] = (U8) MSG_REQ;	
	buf[NTY_PROTO_TYPE_IDX] = NTY_PROTO_DATAPACKET_REQ;
	*(C_DEVID*)(&buf[NTY_PROTO_DATAPACKET_DEVID_IDX]) = (C_DEVID) proto->devid;
	*(C_DEVID*)(&buf[NTY_PROTO_DATAPACKET_DEST_DEVID_IDX]) = toId;
	
	*(U16*)(&buf[NTY_PROTO_DATAPACKET_CONTENT_COUNT_IDX]) = (U16)length;
	length += NTY_PROTO_DATAPACKET_CONTENT_IDX;
	length += sizeof(U32);
	void *pNetwork = ntyNetworkInstance();
	n = ntySendFrame(pNetwork, &proto->serveraddr, buf, length);
#else
	U8 *tempBuf = &buf[NTY_PROTO_DATAPACKET_CONTENT_IDX];
	memcpy(tempBuf, data, length);
	LOG(" toId : %lld \n", toId);
	if (proto && (*protoOpera) && (*protoOpera)->proxyReq) {
		(*protoOpera)->proxyReq(proto, toId, buf, length);
		return 0;
	}
	return -1;
#endif
	
}

int ntySendMassDataPacket(U8 *data, int length) {	
	void *pTree = ntyRBTreeInstance();
	
	LOG(" data:%s, length:%d", data, length);
	ntyFriendsTreeMass(pTree, ntySendDataPacket, data, length);

	return 0;
}

void ntySetSendSuccessCallback(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	proto->onProxySuccess = cb;
}

void ntySetSendFailedCallback(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->onProxyFailed = cb;
	}
}

void ntySetProxyCallback(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->onProxyCallback = cb;
	}
}

void ntySetProxyDisconnect(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->onProxyDisconnect = cb;
	}
}

void ntySetProxyReconnect(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->onProxyReconnect = cb;
	}
}

void ntySetBindResult(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->onBindResult = cb;
	}
}

void ntySetUnBindResult(PROXY_CALLBACK cb) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->onUnBindResult = cb;
	}
}



void ntySetDevId(C_DEVID id) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->devid = id;
	}
}

int ntyGetNetworkStatus(void) {
	void *network = ntyNetworkInstance();
	return ntyGetSocket(network);
}

int ntyStartupClient(void) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		ntySendLogin(proto);
		ntySetupHeartBeatThread(proto); //setup heart proc
		ntySetupRecvProcThread(proto); //setup recv proc
	}

	return ntyGetNetworkStatus();
}

void ntyShutdownClient(void) {
	NattyProto* proto = ntyProtoInstance();
	void *pNetwork = ntyNetworkInstance();
	ntyNetworkRelease(pNetwork);

	proto->u8HeartbeatExistFlag = 1;
	proto->u8RecvExitFlag = 1;
	
	proto->recvThread_id = 0;
	proto->heartbeatThread_id = 0;
	proto->heartbeartRun = 0;
}

#if 1
void ntyBindClient(C_DEVID did) {
	NattyProto* proto = ntyProtoInstance();

	if (proto) {
		ntyProtoClientBind(proto, did);
	}
}

void ntyUnBindClient(C_DEVID did) {
	NattyProto* proto = ntyProtoInstance();

	if (proto) {
		ntyProtoClientUnBind(proto, did);
	}
}
#endif

U8* ntyGetRecvBuffer(void) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		proto->recvBuffer[NTY_PROTO_DATAPACKET_CONTENT_IDX+proto->recvLen] = 0x0;
		return proto->recvBuffer+NTY_PROTO_DATAPACKET_CONTENT_IDX;
	}
	return NULL;
}
#if 1
C_DEVID ntyGetFromDevID(void) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		return proto->fromId;
	}
	return 0x0;
}
#endif

int ntyGetRecvBufferSize(void) {
	NattyProto* proto = ntyProtoInstance();
	if (proto) {
		return proto->recvLen;
	}
	return -1;
}

static void ntySendTimeout(int len) {
	NattyProto* proto = ntyProtoInstance();
	if (proto && proto->onProxyFailed) {
		proto->onProxyFailed(STATUS_TIMEOUT);
	}

	void* pTimer = ntyNetworkTimerInstance();
	ntyStopTimer(pTimer);
}

static void ntyReconnectProc(int len) {
	void *pNetwork = ntyNetworkInstance();
	ntydbg("ntyReconnectProc : %d\n", ntyGetSocket(pNetwork));
	if (-1 == ntyGetSocket(pNetwork)) { //Reconnect failed
		pNetwork = ntyNetworkRelease(pNetwork);
		pNetwork = NULL;
		
	} else { //Reconnect success
		ntyStartupClient();
		//
		void *pConnTimer = ntyReconnectTimerInstance();	
		ntyStopTimer(pConnTimer);

		NattyProto *proto = ntyProtoInstance();
		if (proto->onProxyReconnect) {
			proto->onProxyReconnect(0);
		}
	}
	return ;
}

void ntyReleaseNetwork(void) {
#if 1
	void *network = ntyNetworkInstance();
#endif

	network = ntyNetworkRelease(network);
	network = NULL;

	void *pConnTimer = ntyReconnectTimerInstance();	
	ntyStartTimer(pConnTimer, ntyReconnectProc);
}

C_DEVID* ntyGetFriendsList(int *Count) {
	void *pTree = ntyRBTreeInstance();
	
	C_DEVID *list = ntyFriendsTreeGetAllNodeList(pTree);
	*Count = ntyFriendsTreeGetNodeCount(pTree);

	return list;
}

void ntyReleaseFriendsList(C_DEVID **list) {
	C_DEVID *pList = *list;
	free(pList);
	pList = NULL;
}

static void* ntyRecvProc(void *arg) {
	struct sockaddr_in addr;
	int clientLen = sizeof(struct sockaddr_in);
	NattyProto *proto = arg;
	NattyProtoOpera * const *protoOpera = arg;
	U8 *buf = proto->recvBuffer;

	int ret;
	void *pNetwork = ntyNetworkInstance();

	struct pollfd fds;
	fds.fd = ntyGetSocket(pNetwork);
	fds.events = POLLIN;

	ntydbg(" ntyRecvProc %d\n", fds.fd);
	while (1) {
		if (proto->u8RecvExitFlag){ 
			ntydbg(" ntyRecvProc Exist\n");
			break;
		}
		ret = poll(&fds, 1, 5);
		if (ret) {
			bzero(buf, RECV_BUFFER_SIZE);
			proto->recvLen = ntyRecvFrame(pNetwork, buf, RECV_BUFFER_SIZE, &addr);
			if (proto->recvLen == 0) { //disconnect
				//ntyReconnect(pNetwork);
				//Release Network
				ntyReleaseNetwork();
				
				ntydbg("Prepare to Reconnect to server\n");
				if (proto->onProxyDisconnect) {
					proto->onProxyDisconnect(0);
				}
				break;
			}
			LOG("\n%d.%d.%d.%d:%d, length:%d --> %x\n", *(unsigned char*)(&addr.sin_addr.s_addr), *((unsigned char*)(&addr.sin_addr.s_addr)+1),
				*((unsigned char*)(&addr.sin_addr.s_addr)+2), *((unsigned char*)(&addr.sin_addr.s_addr)+3),													
				addr.sin_port, proto->recvLen, buf[NTY_PROTO_TYPE_IDX]);
			
			
			if (buf[NTY_PROTO_TYPE_IDX] == NTY_PROTO_LOGIN_ACK) {
				int i = 0;
				
				int count = ntyU8ArrayToU16(&buf[NTY_PROTO_LOGIN_ACK_FRIENDS_COUNT_IDX]);
				void *pTree = ntyRBTreeInstance();

				LOG("count : %d", count);
				for (i = 0;i < count;i ++) {
					//C_DEVID friendId = *(C_DEVID*)(&buf[NTY_PROTO_LOGIN_ACK_FRIENDSLIST_DEVID_IDX(i)]);
					C_DEVID friendId = 0;
					ntyU8ArrayToU64(&buf[NTY_PROTO_LOGIN_ACK_FRIENDSLIST_DEVID_IDX(i)], &friendId);
					ntydbg(" friendId i:%d --> %lld\n", i+1, friendId);

					FriendsInfo *friendInfo = ntyRBTreeInterfaceSearch(pTree, friendId);
					if (NULL == friendInfo) {
						FriendsInfo *pFriend = (FriendsInfo*)malloc(sizeof(FriendsInfo));
						assert(pFriend);
						pFriend->sockfd = ntyGetSocket(pNetwork);;
						pFriend->isP2P = 0;
						pFriend->counter = 0;
						pFriend->addr = ntyU8ArrayToU32(&buf[NTY_PROTO_LOGIN_ACK_FRIENDSLIST_ADDR_IDX(i)]);
						pFriend->port = ntyU8ArrayToU16(&buf[NTY_PROTO_LOGIN_ACK_FRIENDSLIST_PORT_IDX(i)]);
						ntyRBTreeInterfaceInsert(pTree, friendId, pFriend);
					} else {
						friendInfo->sockfd = ntyGetSocket(pNetwork);;
						friendInfo->isP2P = 0;
						friendInfo->counter = 0;
						friendInfo->addr = ntyU8ArrayToU32(&buf[NTY_PROTO_LOGIN_ACK_FRIENDSLIST_ADDR_IDX(i)]);
						friendInfo->port = ntyU8ArrayToU16(&buf[NTY_PROTO_LOGIN_ACK_FRIENDSLIST_PORT_IDX(i)]);
					}					
				}
		
				proto->level = LEVEL_DATAPACKET;
				//ntylog("NTY_PROTO_LOGIN_ACK\n");
				
			} else if (buf[NTY_PROTO_TYPE_IDX] == NTY_PROTO_DATAPACKET_REQ) {
				//U16 cliCount = *(U16*)(&buf[NTY_PROTO_DATAPACKET_NOTIFY_CONTENT_COUNT_IDX]);
				U8 data[RECV_BUFFER_SIZE] = {0};//NTY_PROTO_DATAPACKET_NOTIFY_CONTENT_IDX
				U16 recByteCount = ntyU8ArrayToU16(&buf[NTY_PROTO_DATAPACKET_NOTIFY_CONTENT_COUNT_IDX]);
				C_DEVID friId = 0;
				ntyU8ArrayToU64(&buf[NTY_PROTO_DEVID_IDX], &friId);
				U32 ack = ntyU8ArrayToU32(&buf[NTY_PROTO_ACKNUM_IDX]);

				memcpy(data, buf+NTY_PROTO_DATAPACKET_CONTENT_IDX, recByteCount);
				LOG(" recv:%s end\n", data);

				//memset(proto->fromId, 0, sizeof(10));
				memcpy(&proto->fromId, &friId, sizeof(C_DEVID));
				//sendProxyDataPacketAck(friId, ack);
				if (buf[NTY_PROTO_MESSAGE_TYPE] == MSG_RET) {
					if (proto->onProxyFailed)
						proto->onProxyFailed(STATUS_NOEXIST);
					
					continue;
				}
				LOG("proxyAck start");
				if (arg && (*protoOpera) && (*protoOpera)->proxyAck) {
					(*protoOpera)->proxyAck(proto, friId, ack);
				}

				LOG("proxyAck end");
				if (proto->onProxyCallback) {
					proto->recvLen -= (NTY_PROTO_DATAPACKET_CONTENT_IDX+sizeof(U32));
					proto->onProxyCallback(proto->recvLen);
				}
				LOG("onProxyCallback end");
			} else if (buf[NTY_PROTO_TYPE_IDX] == NTY_PROTO_DATAPACKET_ACK) {
				LOG(" send success\n");
				if (proto->onProxySuccess) {
					proto->onProxySuccess(0);
				}
			} else if (buf[NTY_PROTO_TYPE_IDX] == NTY_PROTO_BIND_ACK) {
				int result = ntyU8ArrayToU32(&buf[NTY_PROTO_BIND_ACK_RESULT_IDX]);
				if (proto->onBindResult) {
					proto->onBindResult(result);
				}
				ntydbg(" NTY_PROTO_BIND_ACK\n");
			} else if (buf[NTY_PROTO_TYPE_IDX] == NTY_PROTO_UNBIND_ACK) {
				int result = ntyU8ArrayToU32(&buf[NTY_PROTO_UNBIND_ACK_RESULT_IDX]);
				if (proto->onUnBindResult) {
					proto->onUnBindResult(result);
				}
				ntydbg(" NTY_PROTO_UNBIND_ACK\n");
			}
		}
	}
}






