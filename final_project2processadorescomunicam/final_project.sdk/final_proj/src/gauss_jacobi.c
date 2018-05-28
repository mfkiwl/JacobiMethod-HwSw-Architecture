/*
* Co-Projecto Hw/Sw
*
* Group 11
* Jo�o Ramiro - 81138
* Jos� Vieira - 90900
*
* gauss_jacobi.c
*/




#include "gauss_jacobi.h"

volatile int *sync_f = (int *)0xFFFF0000;



int main() {
	int status;




	//Xil_DCacheDisable();
	Xil_DCacheInvalidate();

	// Disable cache on OCM region
	Xil_SetTlbAttributes(0xFFFF0000,0x14de2);
	// see Xilinx XAPP1079

	/* init DMA in poll mode for simple transfer */
	status = init_XAxiDma_SimplePollMode(DMA_DEV_ID_0, &AxiDma_0);
	if (status != XST_SUCCESS) {
		printf("init_XAxiDma_SimplePollMode: Failed\r\n");
		return XST_FAILURE;
	}

	status = init_XAxiDma_SimplePollMode(DMA_DEV_ID_1, &AxiDma_1);
	if (status != XST_SUCCESS) {
		printf("init_XAxiDma_SimplePollMode: Failed\r\n");
		return XST_FAILURE;
	}

	status = XAxiDma_Simple_MatProd();
	if (status != XST_SUCCESS) {
		printf("XAxiDma_Simple_MatProd: Failed\r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

int XAxiDma_Simple_MatProd(){
	XTime tStart, tEnd;
    int i, status, n_it = 0;
    float x[MATSIZE], normVal = MAX_ERROR + 1;

    /* Points each pointer to the respective base address */
	memA       = (float *)(A_START_ADD);
	memB       = (float *)(B_START_ADD);
	memRes     = (float *)(RES_START_ADD);
	memX       = (float *)(X_START_ADD);
	memA_D_Inv = (float *)(A_D_INV_ADD);


	printf("waiting\n");
	*sync_f = PROC0_STARTED;
	while (*sync_f != PROC1_STARTED)printf("%d\n",*sync_f); // Wait for P1

	printf("moving on\n");

	/* Start measuring time */
    XTime_GetTime(&tStart);

    for (i = 0; i < MATSIZE; i++){
        /* Get the inverse of A(i,i) and
         * apply the "if(i!=j)" in gauss method
         * by putting the diagonal to 0
         */
    	memA_D_Inv[i] = 1.0f/A(i,i);
        A(i,i) = 0;
    }
    Xil_DCacheFlushRange((INTPTR)memA, (unsigned)(MATRIX_SIZE));

    while(normVal > MAX_ERROR && n_it < MAX_IT){

		// send memX
	    status = XAxiDma_SimpleTransfer(&AxiDma_0,(UINTPTR)memX, VECTOR_SIZE, XAXIDMA_DMA_TO_DEVICE);
		if (status != XST_SUCCESS) return XST_FAILURE;
		status = XAxiDma_SimpleTransfer(&AxiDma_1,(UINTPTR)memX, VECTOR_SIZE, XAXIDMA_DMA_TO_DEVICE);
		if (status != XST_SUCCESS) return XST_FAILURE;

		while (XAxiDma_Busy(&AxiDma_0, XAXIDMA_DMA_TO_DEVICE) || XAxiDma_Busy(&AxiDma_1, XAXIDMA_DMA_TO_DEVICE)) { /* Wait for Tx*/ }

		// receive memRes
		Xil_DCacheInvalidateRange((INTPTR)memRes, VECTOR_SIZE);

		status = XAxiDma_SimpleTransfer(&AxiDma_0,(UINTPTR)memRes, VECTOR_SIZE/2, XAXIDMA_DEVICE_TO_DMA);
		if (status != XST_SUCCESS) return XST_FAILURE;
		status = XAxiDma_SimpleTransfer(&AxiDma_1,(UINTPTR)memRes + VECTOR_SIZE/2, VECTOR_SIZE/2, XAXIDMA_DEVICE_TO_DMA);
		if (status != XST_SUCCESS) return XST_FAILURE;

		// send full matrix A
		status = XAxiDma_SimpleTransfer(&AxiDma_0,(UINTPTR)memA, MATRIX_SIZE/2, XAXIDMA_DMA_TO_DEVICE);
		if (status != XST_SUCCESS) return XST_FAILURE;
		status = XAxiDma_SimpleTransfer(&AxiDma_1,(UINTPTR)memA + MATRIX_SIZE/2, MATRIX_SIZE/2, XAXIDMA_DMA_TO_DEVICE);
		if (status != XST_SUCCESS) return XST_FAILURE;

		while (XAxiDma_Busy(&AxiDma_0,XAXIDMA_DMA_TO_DEVICE) || XAxiDma_Busy(&AxiDma_1,XAXIDMA_DMA_TO_DEVICE)) { /* Wait Tx */ }

		while (XAxiDma_Busy(&AxiDma_0,XAXIDMA_DEVICE_TO_DMA) || XAxiDma_Busy(&AxiDma_1,XAXIDMA_DEVICE_TO_DMA)) { /* Wait Rx*/ }

		/*printf("\nMultiplication result(HW) - Res=\n");
		for(i = 0; i < MATSIZE; i++){
			if(fabs(memRes[i]) < 1E-3 && memRes[i]!=0)
				printf("%3.3f ", 1E-3);
			else
				printf("%3.3f ", memRes[i]);
			printf("\n");
		}*/

		normVal = 0;
	    for (i = 0; i < MATSIZE; i++){
	    	x[i] = memX[i];
	    	memX[i] = (float)memA_D_Inv[i] * (memB[i]-memRes[i]);
	    	normVal += (x[i] - memX[i]) * (x[i] - memX[i]);
	    }

	    Xil_DCacheFlushRange((INTPTR)memX, (unsigned)(VECTOR_SIZE));

        n_it++;
	}


    while (*sync_f != PROC1_COMPLETED); // Wait for P1


    /* End measuring time */
    XTime_GetTime(&tEnd);\

    printf("finito\n");
    /* Show results */
    //show_results(memA_D_Inv, n_it, 2*(tEnd-tStart), 1.0*(tEnd-tStart)/(COUNTS_PER_SECOND/1000000));
    return XST_SUCCESS;
}
