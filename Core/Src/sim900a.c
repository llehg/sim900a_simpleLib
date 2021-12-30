/*
 * sim900a.c
 *
 *  Created on: 17 gru 2021
 * Grzegorz Ulfik 2021
 * grupa 2, sekcja 3
 *
 * Projekt obejmuje stworzenie biblioteki do obsługi modemu SIM900.
 */

#include "main.h"
#include "sim900a.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> //atoi

extern UART_HandleTypeDef SIM900_UART;

Sim900Struct SIM;

const char CTRL_Z = 0x1A;


/*
 * @brief Init SIM900 modem
 *
 * @param SIM_Call_Callback pointer to Call callback function (defined by user)
 * @param SIM_SMS_Callback pointer to SMS callback function (defined by user)
 */
void sim900_init(void (*SIM_Call_Callback),void (*SIM_SMS_Callback)){
	SIM.SIM_Call_Callback = SIM_Call_Callback;
	SIM.SIM_SMS_Callback = SIM_SMS_Callback;

}

/*
 * @brief poll function of SIM900 modem, should be called every 100-1000ms
 *
 * @retval modem status (OK 0 , ERROR 1 , MESSAGE_RX 2 , RINGING 3
 */
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
	if(SIM.timeout < 20 && SIM.timeout > 1)return SIMREPLY_WAITING; //return if reply not received yet
	if(SIM.gprsTimeout){
		SIM.gprsTimeout --;
		return SIMREPLY_WAITING;
	}


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

#ifdef INCLUDE_GPRS

	case AT_CLOSE_GPRS:
			txlen = sprintf(txbuffer, "AT+CIPSHUT\r\n");

			break;

	case AT_START_IP:
			txlen = sprintf(txbuffer, "AT+CIPMUX=0\r\n");

			break;

	case AT_SET_APN:
			txlen = sprintf(txbuffer, "AT+CSTT=\"%s\"\r\n",SIM.APN);
			break;

	case AT_BRING_CONN:
			txlen = sprintf(txbuffer, "AT+CIICR\r\n");
			SIM.gprsTimeout = 50;
			break;

	case AT_GET_IP:
			txlen = sprintf(txbuffer, "AT+CIFSR\r\n");
			break;

	case AT_SELECT_IPMODE:
			txlen = sprintf(txbuffer, "AT+CIPSPRT=0\r\n");
			break;

	case AT_START_CONN:
			txlen = sprintf(txbuffer, "AT+CIPSTART=\"TCP\",\"%s\",\"80\"\r\n",SIM.requestHost);
			SIM.gprsTimeout = 50;
			break;

	case AT_BEGIN_REQ:
			txlen = sprintf(txbuffer, "AT+CIPSEND\r\n");
			SIM.gprsTimeout = 50;
			break;

	case AT_SEND_REQ:
			txlen = sprintf(txbuffer, "GET %s\r\n",SIM.requestUrl);
					SIM.gprsTimeout = 50;
					break;

#endif

	default:
		txlen = sprintf(txbuffer, "AT\r\n");
		break;
	}

	HAL_UART_Transmit(&SIM900_UART, (uint8_t*) txbuffer, txlen, 100);
	if(SIM.next_pollReq == AT_SEND_REQ)HAL_UART_Transmit(&SIM900_UART, (uint8_t*) &CTRL_Z, 1, 200);

	if(SIM.timeout > DECLARED_TIMEOUT)return SIMREPLY_ERROR;
	return SIMREPLY_OK;
}

/*
 * @brief bufor parser of SIM900
 *
 * @param data pointer to received data
 * @param len length of received data
 */
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

#ifdef INCLUDE_GPRS
	case AT_CLOSE_GPRS:
		if(SIM.gprsProcessing == 0)SIM.next_pollReq = AT_SIGNAL_STRENGTH;
		else if (strncmp(strings[1], "SHUT OK", 7) == 0) {
					SIM.next_pollReq = AT_START_IP;
				}
		break;

	case AT_START_IP:
		if (strncmp(strings[1], "OK", 2) == 0) {
					SIM.next_pollReq = AT_SET_APN;
				}
		break;

	case AT_SET_APN:
		if (strncmp(strings[1], "OK", 2) == 0) {
					SIM.next_pollReq = AT_BRING_CONN;
				}
		break;

	case AT_BRING_CONN: //typical very long reply
		if (strncmp(strings[0], "OK", 2) == 0) {
					SIM.next_pollReq = AT_GET_IP;
					SIM.gprsTimeout = 0;
			}
		break;

	case AT_GET_IP: //ToDo: verify if received ip, not trash
			SIM.next_pollReq = AT_SELECT_IPMODE;
		break;

	case AT_SELECT_IPMODE:
		if (strncmp(strings[1], "OK", 2) == 0) {
					SIM.next_pollReq = AT_START_CONN;
				}
		break;

	case AT_START_CONN: //typical long reply
		if (strncmp(strings[0], "CONNECT OK", 10) == 0) {
					SIM.next_pollReq = AT_BEGIN_REQ;
					SIM.gprsTimeout = 0;
				}
		break;

	case AT_BEGIN_REQ:

		SIM.next_pollReq = AT_SEND_REQ;
		SIM.gprsTimeout = 0;

		break;

	case AT_SEND_REQ:


		if (strncmp(strings[0], "SEND OK", 7) == 0) {
					SIM.gprsProcessing = 100;
				}
		else if(SIM.gprsProcessing == 100){
			SIM.SIM_GPRS_Callback(strings[0]);
			SIM.gprsProcessing = 1;
		}

		if (strncmp(strings[0], "CLOSED", 6) == 0) {
			SIM.gprsProcessing = 0;
			SIM.gprsTimeout = 0;
			SIM.next_pollReq = AT_CLOSE_GPRS;
		}

		break;

#endif

	default:
		if(SIM.messageSendStatus)SIM.next_pollReq = AT_MESSAGEFORMAT; //if message waiting to send, send
		else if(SIM.gprsProcessing)SIM.next_pollReq = AT_CLOSE_GPRS;
		else SIM.next_pollReq = AT_SIGNAL_STRENGTH; //back to loop
		break;
	}
}

/*
 *
 * @param number String pointer, number in format "48000112233"
 * @param message String message
 */
void sim900_sendSMS(char *number, char *message) {
	strncpy(SIM.txMessageNumber, number, 12);
	strncpy(SIM.txMessage, message, 160);
	SIM.messageSendStatus = 1;
}

/*
 * @brief Get SIM900 struct (with current parameters)
 *
 * @note If user specify the parameter pointer, function will overwrite it with SIM900 struct adress
 *
 * @param pointer pointer to Call callback function (defined by user)
 * @retval SIM900 struct
 */
Sim900Struct sim900_getAllParameters(Sim900Struct** pointer){ //pointer to pointer not by mistake
	if(pointer)*pointer = &SIM; //return pointer if pointer specified
	return SIM;
}

#ifdef INCLUDE_GPRS
void sim900_setAPN(char* apn){
	strncpy(SIM.APN, apn , GPRS_APN_LEN);
}


void sim900_getGPRS(char* host, char* url, void (*Callback)(char *reply)){
	if(SIM.gprsProcessing)return; //if another process, abort request
	SIM.gprsProcessing = 2;
	SIM.SIM_GPRS_Callback = Callback;
	strncpy(SIM.requestHost, host, GPRS_HOST_LEN);
	strncpy(SIM.requestUrl , url, GPRS_REQ_LEN);

}
#endif
