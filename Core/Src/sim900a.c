/*
 * sim900a.c
 *
 *  Created on: 17 gru 2021
 *      Author: sq9ril
 */

#include "main.h"
#include "sim900a.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> //atoi

extern UART_HandleTypeDef SIM900_UART;

Sim900Struct SIM;

const char CTRL_Z = 0x1A;


void sim900_init(void (*SIM_Call_Callback),void (*SIM_SMS_Callback)){
	SIM.SIM_Call_Callback = SIM_Call_Callback;
	SIM.SIM_SMS_Callback = SIM_SMS_Callback;

}

uint8_t sim900_poll(void) {
	SIM.timeout++;

	if (SIM.recaivedMessage) {
		SIM.recaivedMessage--;
		if (SIM.SIM_SMS_Callback)
			SIM.SIM_SMS_Callback(SIM.rxMessageNumber, SIM.rxMessage);
		return SIMREPLY_MESSAGE_RX;
	}

	if(SIM.callingRings){
		if(SIM.SIM_Call_Callback)
			SIM.SIM_Call_Callback(SIM.callingNumber, SIM.callingRings);
		SIM.callingRings = 0;
		return SIMREPLY_RINGING;
	}

	//Polling modem

	char txbuffer[200];
	uint8_t txlen;

	switch (SIM.next_pollReq) {
	case AT_SIGNAL_STRENGTH:
		txlen = sprintf(txbuffer, "AT+CSQ\r\n");
		break;

	case AT_REGISTERINFO:
		txlen = sprintf(txbuffer, "AT+CREG?\r\n");
		break;
	case AT_CHECKNUMBER:
		txlen = sprintf(txbuffer, "AT+CNUM\r\n");
		break;
	case AT_ENABLE_CLIP:
		txlen = sprintf(txbuffer, "AT+CLIP=1\r\n");
		break; //informacja o dzwoniacym

	case AT_MESSAGEFORMAT:
		txlen = sprintf(txbuffer, "AT+CMGF=1\r\n");
		break;
	case AT_SELECTCHARACTERSET:
		txlen = sprintf(txbuffer, "AT+CSCS=\"GSM\"\r\n");
		break;
	case AT_SENDSMS:
		txlen = sprintf(txbuffer, "AT+CMGS=\"+%s\"\r", SIM.txMessageNumber);
		break;

	case AT_DOWNLOAD_MESSAGE:
		txlen = sprintf(txbuffer, "AT+CMGR=1\r\n");
		break;

	case AT_DELETE_SMS:
		txlen = sprintf(txbuffer, "AT+CMGDA=\"DEL READ\"\r\n");
		break;

	default:
		txlen = sprintf(txbuffer, "AT\r\n");
		break;
	}

	HAL_UART_Transmit(&SIM900_UART, (uint8_t*) txbuffer, txlen, 100);

	if(SIM.timeout > DECLARED_TIMEOUT)return SIMREPLY_ERROR;
	return SIMREPLY_OK;
}

void sim900_parseReply(uint8_t* data, uint16_t len){
	if (len < 2)
		return; //minimum reply length

	char *strings[3] = {0,0,0}; //setting to 0 prevents hardfault if string is not present, but pointer not 0
	char *temp;

	SIM.timeout = 0;

	for (uint8_t i = 0; i < len; i++) {
		if (data[i] == '\r' || data[i] == '\n')
			data[i] = 0;
	}

	if (strncmp((char*) &data[2], "RING", 4) == 0) {
		SIM.callingRings++;
		return;
	}

	uint8_t string_n = 0;

	if (data[0]) {
		strings[0] = (char*) &data[0];
		string_n++;
	}
	for (uint8_t i = 0; i < len; i++) {
		if (data[i] == 0 && data[i + 1]) {
			strings[string_n] = (char*) &data[i + 1];
			if (string_n < 3)
				string_n++;
		}
	}
	data[len] = 0;

	if (strncmp((char*) &data[2], "+CMGR:", 6) == 0) { //copy incoming sms
		temp = strchr(strings[0] + 1, '+');
		strncpy(SIM.rxMessageNumber, temp, 12);
		strncpy(SIM.rxMessage, strings[1], 160);
		SIM.recaivedMessage++;

#ifdef DEBUG_PRINTF
	  printf("<Sim900 SMS in %s %s\n", SIM.rxMessageNumber, SIM.rxMessage);
#endif

		return;
	}

	if (strncmp((char*) &data[2], "+CLIP:", 6) == 0) { //calling number info
		temp = strchr((char*) &data[5] , '+');
		strncpy(SIM.callingNumber, temp, 12);
		return;
	}

	if (strncmp((char*) &data[2], "+CMTI:", 6) == 0) {

		return;
	}

#ifdef DEBUG_PRINTF
	if(strings[0])printf("<Sim900 req:%s \n", strings[0]);
	if(strings[1])printf("reply:%s \n", strings[1]);
#endif

	if (*strings[1] == '>' && SIM.next_pollReq == AT_SENDSMS) {
		HAL_UART_Transmit(&SIM900_UART, (uint8_t*) SIM.txMessage,
				strlen(SIM.txMessage), 200);
		HAL_UART_Transmit(&SIM900_UART, (uint8_t*) &CTRL_Z, 1, 200);
		return;
	}

	//Regular poll requests
	switch (SIM.next_pollReq) {
	//One time req
	case AT_TEST:
		if (strncmp(strings[1], "OK", 2) == 0) {
			SIM.next_pollReq = AT_ENABLE_CLIP;
		}
		break;

	case AT_ENABLE_CLIP:
		if (strncmp(strings[1], "OK", 2) == 0) {
			SIM.next_pollReq = AT_CHECKNUMBER;

		}
		break;


	case AT_CHECKNUMBER:
		if (strncmp(strings[1], "+CNUM:", 6) == 0) {
			temp = strchr(strings[1] + 1, '+');
			strncpy(SIM.simNumber, temp, 12);
			SIM.next_pollReq = AT_SIGNAL_STRENGTH;
		}
		break;

		//Req loop
	case AT_SIGNAL_STRENGTH:
		if (strncmp(strings[1], "+CSQ:", 5) == 0) {
			SIM.signalStrength = atoi((char*) (strings[1] + 5));

			SIM.timeout = 0;
			SIM.next_pollReq = AT_REGISTERINFO;
		}

		break;

	case AT_REGISTERINFO:
		if (strncmp(strings[1], "+CREG:", 6) == 0) {
			temp = strchr(strings[1], ',');
			SIM.simRegistered = atoi(temp + 1);
			SIM.next_pollReq = AT_DELETE_SMS;
		}
		break;

	case AT_DELETE_SMS:
		if (strncmp(strings[1], "OK", 2) == 0) {
			SIM.next_pollReq = AT_DOWNLOAD_MESSAGE;
		}
		break;

	case AT_MESSAGEFORMAT:

		SIM.next_pollReq = AT_SELECTCHARACTERSET;
		break;

	case AT_SELECTCHARACTERSET:
		SIM.next_pollReq = AT_SENDSMS;
		break;

	case AT_SENDSMS:
		SIM.next_pollReq = AT_SIGNAL_STRENGTH; //back to loop
		SIM.messageSendStatus = 0;
		break;

	default:
		if(SIM.messageSendStatus)SIM.next_pollReq = AT_MESSAGEFORMAT; //if message waiting to send, send
					else SIM.next_pollReq = AT_SIGNAL_STRENGTH; //back to loop
		break;
	}
}

void sim900_sendSMS(char *number, char *message) {
	strncpy(SIM.txMessageNumber, number, 12);
	strncpy(SIM.txMessage, message, 160);
	SIM.messageSendStatus = 1;
}

Sim900Struct sim900_getAllParameters(Sim900Struct** pointer){ //pointer to pointer not by mistake
	if(pointer)*pointer = &SIM; //return pointer if pointer specified
	return SIM;
}
