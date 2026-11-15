/*
 * my_httpd.c
 *
 *  Created on: 8 maj 2022
 *      Author: olek
 */

#include "my_httpd.h"
#include <string.h>
#include "lwip/apps/httpd.h"
#include "my_log.h"

bool LD1ON = false; // this variable will indicate if the LD3 LED on the board is ON or not
bool LD2ON = false; // this variable will indicate if our LD2 LED on the board is ON or not


// just declaring the function for the compiler [= CGI #2 =]
const char* LedCGIhandler(int iIndex, int iNumParams, char *pcParam[],
		char *pcValue[]);

// in our SHTML file <form method="get" action="/leds.cgi"> [= CGI #3 =]
const tCGI LedCGI = { "/leds.cgi", LedCGIhandler };

// [= CGI #4 =]
tCGI theCGItable[1];

// just declaring SSI handler function [* SSI #1 *]
u16_t mySSIHandler(int iIndex, char *pcInsert, int iInsertLen);

// [* SSI #2 *]
#define numSSItags (LOG_MAX + 2)

// [* SSI #3 *]
char const *theSSItags[numSSItags] = {
	"tag1", "tag2",
	"w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10",
	"w11", "w12", "w13", "w14", "w15", "w16", "w17", "w18", "w19", "w20",
	"w21", "w22", "w23", "w24", "w25", "w26", "w27", "w28", "w29", "w30",
	"w31", "w32", "w33", "w34", "w35", "w36", "w37", "w38", "w39", "w40",
	"w41", "w42", "w43", "w44", "w45", "w46", "w47", "w48", "w49", "w50"
};

// the actual function for handling CGI [= CGI #5 =]
const char* LedCGIhandler(int iIndex, int iNumParams, char *pcParam[],
		char *pcValue[]) {
	uint32_t i = 0;
	if (iIndex == 0) {
		//turning the LED lights off
		HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
		// we put this variable to false to indicate that the LD2 LED on the board is not ON
		LD2ON = false;
		HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
		// we put this variable to false to indicate that the LD* LED on the board is not ON
		LD1ON = false;
		//mqtt_my_publish(&mqtt_client, "Led1", (char)LD1ON);
	}
	for (i = 0; i < iNumParams; i++) {
		if (strcmp(pcParam[i], "led") == 0) {
			if (strcmp(pcValue[i], "1") == 0) {
				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
				// LD3 LED (red) on the board is ON!
				LD1ON = true;
			} else if (strcmp(pcValue[i], "2") == 0) {
				HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
				// LD2 LED (blue) on the board is ON!
				LD2ON = true;
			}
		}
	}
	// the extension .shtml for SSI to work
	return "/index.shtml";
} // END [= CGI #5 =]

// function to initialize CGI [= CGI #6 =]
void myCGIinit(void) {
	//add LED control CGI to the table
	theCGItable[0] = LedCGI;
	//give the table to the HTTP server
	http_set_cgi_handlers(theCGItable, 1);
} // END [= CGI #6 =]

// the actual function for SSI [* SSI #4 *]
u16_t mySSIHandler(int iIndex, char *pcInsert, int iInsertLen) {
	if (iIndex == 0) {
		if (LD1ON == false) {
			char myStr1[] = "<input value=\"1\" name=\"led\" type=\"checkbox\">";
			strcpy(pcInsert, myStr1);
			return strlen(myStr1);
		} else if (LD1ON == true) {
			// since the LD3 red LED on the board is ON we make its checkbox checked!
			char myStr1[] =
					"<input value=\"1\" name=\"led\" type=\"checkbox\" checked>";
			strcpy(pcInsert, myStr1);
			return strlen(myStr1);
		}
	} else if (iIndex == 1) {
		if (LD2ON == false) {
			char myStr2[] = "<input value=\"2\" name=\"led\" type=\"checkbox\">";
			strcpy(pcInsert, myStr2);
			return strlen(myStr2);
		} else if (LD2ON == true) {
			// since the LD2 blue LED on the board is ON we make its checkbox checked!
			char myStr2[] =
					"<input value=\"2\" name=\"led\" type=\"checkbox\" checked>";
			strcpy(pcInsert, myStr2);
			return strlen(myStr2);
		}
	} else if (iIndex >= 2 && iIndex < numSSItags) {
		log_get(iIndex - 2, pcInsert, (size_t)iInsertLen);
		return strlen(pcInsert);
	}
	return 0;
}

// function to initialize SSI [* SSI #5 *]
void mySSIinit(void) {

	http_set_ssi_handler(mySSIHandler, (char const**) theSSItags, numSSItags);
}
