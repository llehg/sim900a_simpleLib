/*
 * sim900a.h
 *
 *  Created on: 17 gru 2021
 * Grzegorz Ulfik 2021
 * grupa 2, sekcja 3
 *
 * Projekt obejmuje stworzenie biblioteki do obsługi modemu SIM900.
 *
 *      SIM900 FW: 1137B09SIM900B32_ST
 */

#ifndef INC_SIM900A_H_
#define INC_SIM900A_H_

#define DEBUG_PRINTF

#define INCLUDE_GPRS

#define SIM900_UART huart3
#define SIM900_UARTLENGTH 240
#define DECLARED_TIMEOUT 100

//GRPS declarations
#define GPRS_APN_LEN 16
#define GPRS_HOST_LEN 32
#define GPRS_REQ_LEN 64
#define GPRS_REPLY_LEN 64


void sim900_init(void (*SIM_Call_Callback),void (*SIM_SMS_Callback));
uint8_t sim900_poll(void);
void sim900_parseReply(uint8_t* data, uint16_t len);
void sim900_sendSMS(char* number, char* message);

#ifdef INCLUDE_GPRS
void sim900_setAPN(char* apn);
void sim900_getGPRS(char* host, char* url, void (*Callback)(char *reply));
#endif


typedef struct _Sim900Struct {
	//Modem informations
	uint8_t next_pollReq;
	uint8_t signalStrength;
	uint8_t simRegistered;
	uint8_t timeout;
	char simNumber[12];

	//Call
	char callingNumber[12];
	uint8_t callingRings;

	//SMS TX
	uint8_t messageSendStatus;
	char txMessage[160];
	char txMessageNumber[12];

	//SMS RX
	uint8_t recaivedMessage;
	char rxMessage[160];
	char rxMessageNumber[12];

#ifdef INCLUDE_GPRS
	//GPRS
	uint8_t gprsProcessing;
	uint8_t gprsTimeout;
	uint8_t gprsReply;
	char APN[GPRS_APN_LEN];
	char requestHost[GPRS_HOST_LEN];
	char requestUrl[GPRS_REQ_LEN];
	char requestReply[GPRS_REPLY_LEN];
	void (*SIM_GPRS_Callback)(char *reply);
#endif

	//Callbacks
	void (*SIM_Call_Callback)(char *callingNumber, uint8_t rings);
	void (*SIM_SMS_Callback)(char *messagingNumber, char *text);

} Sim900Struct;

Sim900Struct sim900_getAllParameters(Sim900Struct** pointer);

#define SIMREPLY_OK 0
#define SIMREPLY_ERROR 1
#define SIMREPLY_MESSAGE_RX 2
#define SIMREPLY_RINGING 3
#define SIMREPLY_WAITING 4



#define AT_TEST 0
#define AT_ENABLE_CLIP 1
#define AT_CHECKNUMBER 2

#define AT_SIGNAL_STRENGTH 10
#define AT_REGISTERINFO 11
#define AT_DELETE_SMS 12
#define AT_DOWNLOAD_MESSAGE 13

#define AT_MESSAGEFORMAT 100
#define AT_SELECTCHARACTERSET 101
#define AT_SENDSMS 102
#define SENDSMS_TEXT 103

#define AT_CLOSE_GPRS 120
#define AT_START_IP 121
#define AT_SET_APN 122
#define AT_BRING_CONN 123
#define AT_GET_IP 124
#define AT_SELECT_IPMODE 125
#define AT_START_CONN 126
#define AT_BEGIN_REQ 127
#define AT_SEND_REQ 128



#endif /* INC_SIM900A_H_ */
