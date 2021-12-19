/*
 * sim900a.h
 *
 *  Created on: 17 gru 2021
 *      Author: sq9ril
 */

#ifndef INC_SIM900A_H_
#define INC_SIM900A_H_

#define DEBUG_PRINTF
#define SIM900_UART huart3
#define SIM900_UARTLENGTH 240
#define DECLARED_TIMEOUT 100


void sim900_init(void (*SIM_Call_Callback),void (*SIM_SMS_Callback));
uint8_t sim900_poll(void);
void sim900_parseReply(uint8_t* data, uint16_t len);
void sim900_sendSMS(char* number, char* message);


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

	//Callbacks
	void (*SIM_Call_Callback)(char *callingNumber, uint8_t rings);
	void (*SIM_SMS_Callback)(char *messagingNumber, char *text);

} Sim900Struct;

Sim900Struct sim900_getAllParameters(Sim900Struct** pointer);

#define SIMREPLY_OK 0
#define SIMREPLY_ERROR 1
#define SIMREPLY_MESSAGE_RX 2
#define SIMREPLY_RINGING 3



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



#endif /* INC_SIM900A_H_ */
