/*
 ============================================================================
 Name        : DaveHelloWorld.c
 Author      : DAS
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	puts("!!!Hello World Dave!!!"); /* prints !!!Hello World!!! */
	puts("!!!Hello World Dave3!!!"); /* prints !!!Hello World!!! */
	puts("!!!Hello World Dave2!!!"); /* prints !!!Hello World!!! */
	puts("!!!Hello World Dave3!!!"); /* prints !!!Hello World!!! */
	puts("!!!Hello World Dave3!!!"); /* prints !!!Hello World!!! */
	puts("!!!Hello World Dave4!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
