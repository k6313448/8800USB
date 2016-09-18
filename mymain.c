/***********************************************************************
    > File Name: main.c
    > Author: maoky
    > Mail: maoky@bellnett.com 
    > Created Time: Wed 7 Sep 2016 2:51:13 PM CST
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>
#include <fcntl.h>
#include <sys/socket.h>

libusb_context *context = NULL;
struct libusb_device_handle *devh = NULL;
unsigned char recv_buf[256];
struct libusb_transfer *transfer;
volatile int bulk_in = 0x82;
volatile int bulk_out = 0x02;
#define MAC_LEN 6

//init the usb device
int init_usb_dev(){
	struct libusb_device **devs;
	int ret, i, cnt, r = 0;
    ret = libusb_init(&context);
    if (ret < 0)
    {
        fprintf(stderr, "failed to initialise libusb\n");
        return 1;
    }

    libusb_set_debug(context, 3);
    cnt = libusb_get_device_list(context, &devs);
    if (cnt < 0)
    {
        printf("get libusb list error!\n");
        return 1;
    }
    printf("===================device list cnt:%d \n", cnt);
    for(i=0; i<cnt; i++)
    {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0)
        {
            printf("fail to get device descriptor!\n");
            return 1;
        }
        printf("VendorID:0x%04X, ProductID:0x%04X\n", desc.idVendor,desc.idProduct);
        if(desc.idVendor == 0x1a86)
        {
            printf("open c2640 usb\n");
            devh =libusb_open_device_with_vid_pid(context, desc.idVendor,desc.idProduct);
        }
    }
    if (!devh)
    {
        fprintf(stderr, "Unable to open device.\n");
        return 1;
    }
    /* claim interface */
    ret = libusb_claim_interface(devh, 0);
    if (ret < 0)
    {
        perror("libusb_claim_interface\n");
        devh = NULL;
        libusb_close(devh);
        return ret;
    }
    return 2;
}  
  
void cb_response_in(struct libusb_transfer *transfer){
	printf("read_pthread call_back!\n");
    //printf("Received data: %c\n", recv_buf[0]);
    //char Mac[6];
    //memcpy(Mac,recv_buf,MAC_LEN);
    int j=0;
    for(j=0; j<40; j++){
        printf("%x", recv_buf[j]);
    }
    printf("\n");
    
    printf("Mac: ");
    for(j=0; j<MAC_LEN; j++){
        printf("%x", recv_buf[j]);
    }
    printf("\n");
    
    fprintf(stderr, "cb_response_in: status =%d, actual_length=%d\n", transfer->status, transfer->actual_length);
    printf("%d", transfer->flags);
    printf("%d", transfer->timeout);

    /*
    printf("Length: ");
    printf("%x", recv_buf[MAC_LEN]);
    printf("\n");
    printf("Data: ");
    int data_length = (int)(recv_buf[MAC_LEN]);
    for(j=0; j<data_length; j++){
        printf("%x", recv_buf[MAC_LEN+j+1]);
    }
    printf("\n");
    printf("RSSI: ");
    printf("%x", recv_buf[MAC_LEN+data_length+1]);*/
    printf("\n");
    printf("\n");

    int ret = libusb_submit_transfer(transfer);
    if (ret != 0)
    {
        printf("error libusb_submit_transfer: %d\n", ret);
    }
    memset(recv_buf,0,256);
    return;
}

//read data from usb
int read_pthread(){
    int ret;
    //memset(recv_buf, 0, sizeof(recv_buf));
    transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(transfer, devh, bulk_in, recv_buf, 256, cb_response_in, NULL, 0);
    ret = libusb_submit_transfer(transfer);
    if (ret != 0)
    {
        printf("error libusb_submit_transfer: %d\n", ret);
    }
    return ret;
}

//////////////////////
int main(int argc, char *argv[]){
	int ret, r_init, r_open;
    pthread_t readusb;
    r_init = init_usb_dev();
    ret = pthread_create(&readusb, NULL, (void *) read_pthread, NULL);
    if (ret){
        printf("create read_pthread error!\n");
        return 1;
    }
    printf("suspend\n");
    while(1){
        struct timeval tv;
        tv.tv_sec = 0;          // seconds
        tv.tv_usec = 100;       // milliseconds  ( .1 sec)

        ret = libusb_handle_events_timeout(context, &tv);
        if (ret < 0)
        {                       // negative values are errors
            printf("Time out!\n");;
        }
    }
    pthread_join(readusb, NULL);
    libusb_release_interface(devh, 0);
    libusb_exit(context);
    return 0;
}



