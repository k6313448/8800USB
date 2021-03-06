#include <stdio.h>     
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h>   
#include <sys/stat.h>     
#include <fcntl.h>  
#include <termios.h> 
#include <errno.h>
#include <string.h>  
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <json-c/json.h>
#include "utils.h"
#include "list.h"

int interval_g;
int cachesize_g;
int totalcnt_g;
volatile int tolcnt_g = 0;
char IP[32], PORT[8], PATH[128];

typedef struct{
	char sn[17];
	char mac[13];
	char rssi[3];
	char stim[16]; 
} __attribute__ ((packed)) usb_data_send;

typedef struct usb_send_buf{
	usb_data_send data;
	struct usb_send_buf *next;
} __attribute__ ((packed)) usb_buf_send;

////////serial port==>////////
int UART0_Open(char* port) {  
	int fd;

	fd = open(port, O_RDWR|O_NOCTTY|O_NDELAY);  
	if (FALSE == fd) {  
		// perror("Can't Open Serial Port");  
		return(FALSE);  
	}  

	if(fcntl(fd, F_SETFL, 0) < 0) {  
		LOGE("fcntl failed!\n");  
		return(FALSE);  
	} else {  
		// printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));  
	}  
	/*
	if(0 == isatty(STDIN_FILENO))  
	{  
		printf("standard input is not a terminal device\n");  
		return(FALSE);  
	}  
	else  
	{  
		printf("isatty success!\n");  
	} */               
	// printf("fd->open=%d\n",fd);  
	return fd;  
}  


void UART0_Close(int fd) {  
	close(fd);  
}  

int UART0_Set(int fd,int speed,int flow_ctrl,int databits,int stopbits,int parity) {  
	int   i;  
	int   speed_arr[] = { B115200, B19200, B9600, B4800, B2400, B1200, B300};  
	int   name_arr[] = {115200,  19200,  9600,  4800,  2400,  1200,  300};  

	struct termios options;  

	if  ( tcgetattr( fd,&options)  !=  0) {  
		// perror("SetupSerial 1");      
		return(FALSE);   
	}  

	for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) {  
		if  (speed == name_arr[i]) {               
			cfsetispeed(&options, speed_arr[i]);   
			cfsetospeed(&options, speed_arr[i]);    
		}  
	}       

	options.c_cflag |= CLOCAL;  
	options.c_cflag |= CREAD;  

	switch(flow_ctrl) {  
		case 0 :
			options.c_cflag &= ~CRTSCTS;  
			break;     
		case 1 :
			options.c_cflag |= CRTSCTS;  
			break;  
		case 2 :  
			options.c_cflag |= IXON | IXOFF | IXANY;  
			break;  
	}  

	options.c_cflag &= ~CSIZE;  
	switch (databits) {    
		case 5    :  
			options.c_cflag |= CS5;  
			break;  
		case 6    :  
			options.c_cflag |= CS6;  
			break;  
		case 7    :      
			options.c_cflag |= CS7;  
			break;  
		case 8:      
			options.c_cflag |= CS8;  
			break;    
		default:     
			fprintf(stderr,"Unsupported data size\n");  
			return (FALSE);   
	}  

	switch (parity) {    
		case 'n':  
		case 'N':
			options.c_cflag &= ~PARENB;   
			options.c_iflag &= ~INPCK;      
			break;   
		case 'o':    
		case 'O':
			options.c_cflag |= (PARODD | PARENB);   
			options.c_iflag |= INPCK;               
			break;   
		case 'e':   
		case 'E':
			options.c_cflag |= PARENB;         
			options.c_cflag &= ~PARODD;         
			options.c_iflag |= INPCK;        
			break;  
		case 's':  
		case 'S': 
			options.c_cflag &= ~PARENB;  
			options.c_cflag &= ~CSTOPB;  
			break;   
		default:    
			fprintf(stderr,"Unsupported parity\n");      
			return (FALSE);   
	}   

	switch (stopbits) {    
		case 1:     
			options.c_cflag &= ~CSTOPB; break;   
		case 2:     
			options.c_cflag |= CSTOPB; break;  
		default:     
			fprintf(stderr,"Unsupported stop bits\n");   
			return (FALSE);  
	}  


	options.c_oflag &= ~OPOST;  
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	//options.c_lflag &= ~(ISIG | ICANON);

	options.c_cc[VTIME] = 1; 
	options.c_cc[VMIN] = 1;

	tcflush(fd,TCIFLUSH);  

	if (tcsetattr(fd,TCSANOW,&options) != 0) {  
		// perror("com set error!\n");    
		return (FALSE);   
	}  
	return (TRUE);   
}  

int UART0_Init(int fd, int speed,int flow_ctrl,int databits,int stopbits,int parity) {  
	if (UART0_Set(fd,115200,0,8,1,'N') == FALSE) {                                                           
		return FALSE;  
	} else {  
		return  TRUE;  
	}  
}  


int UART0_Recv(int fd, char *rcv_buf,int data_len) {  
	int len, fs_sel;  
	fd_set fs_read;  

	struct timeval time;  

	FD_ZERO(&fs_read);  
	FD_SET(fd, &fs_read);  

	time.tv_sec = 10;
	time.tv_usec = 0;

	fs_sel = select(fd+1, &fs_read, NULL, NULL, &time);  
	if(fs_sel) {  
		len = read(fd, rcv_buf, data_len);  
		// printf("Recv Success! len = %d fs_sel = %d\n",len,fs_sel);  
		return len;  
	} else {  
		// LOGE("Recv Fail!\n");  
		return FALSE;  
	}       
}  

int UART0_Send(int fd, char *send_buf,int data_len) {  
	int len = 0;  
	len = write(fd, send_buf, data_len);  
	if (len == data_len ) {  
		return len;  
	} else {  
		tcflush(fd, TCOFLUSH);  
		return FALSE;  
	}  
}  
////////<==serial port////////

////////Make Json & Send Data////////
static int create_and_connect(const char *addr, const char *port, int type, struct sockaddr *ser) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, listen_sock;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = type; /* We want a TCP socket */

	s = getaddrinfo(addr, port, &hints, &result);
	if (s != 0) {
		LOGE("getaddrinfo: %s", gai_strerror(s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (listen_sock == -1) {
			continue;
		}
		if(SOCK_STREAM == type) {
			int opt = 1;
			setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

			s = connect(listen_sock, rp->ai_addr, rp->ai_addrlen);
			if (s == 0) {
				/* We managed to connect successfully! */
				// printf("socket connect success.\n");
				break;
			}
		} else if(SOCK_DGRAM == type) {
			if(ser)
				memcpy(ser, rp->ai_addr, rp->ai_addrlen);
			break;
		}
		close(listen_sock);
	}

	if (rp == NULL) {
		LOGE("Could not connect to server");
		return -1;
	}

	freeaddrinfo(result);
	return listen_sock;
}

int build_http_header(char* http_header, char* ip,char* port, char* path, char * data, int len)
{
	int j;
	char head[128];

	if(!strcmp(port, "80")) {
		snprintf(head, sizeof(head), "POST http://%s/%s HTTP/1.1\r\n", ip, path);
	} else {
		snprintf(head, sizeof(head), "POST http://%s:%s/%s HTTP/1.1\r\n", ip, port, path);
	}
	
	j = snprintf(http_header, 512 + len,"%s\
Accept: */*\r\n\
Accept-Language: zh-cn\r\n\
User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n\
Host: %s\r\n\
Referer: http://%s\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Pragma: no-cache\r\n\
Connection: Keep-Alive\r\n\
Content-Length: %d\r\n\r\n%s\n",head,ip,ip,len,data);

	return j;
}

static int http_send_to_server(char *ip, char *port, char *path, char *buf) {
	int fd, r, len;
	int cnt = 0;
	char *sendbuf;// *recvbuf;
	len = strlen(buf);
	sendbuf = malloc(512 + len);
	build_http_header(sendbuf, ip, port, path, buf, len);
	len = strlen(sendbuf);
	// printf("Final Data: \n%s\n", sendbuf);
	fd = create_and_connect(ip, port, SOCK_STREAM, NULL);
	//r = send(fd, sendbuf, len, 0);
	while (1) {
		r = send(fd, sendbuf,len, 0);
		// printf("r = %d\n",r);
		if(r < 0) {
			LOGE("Send Failed!\n");
			break;
			if (errno == EINTR) {
				continue;
			} else {
				LOGE("Send Failed!\n");
				if(++cnt > 3) {
					break;
				} else {
					close(fd);
					fd = create_and_connect(ip,port, SOCK_STREAM, NULL);
					continue;
				}
			}
		}
		else if(r > 0 && r < len) {
			LOGE("Send partly!\n");
			sendbuf += r;
			len -= r;
			continue;
		} else {
			LOGE("Send Finished!\n");
			break;
		}
	}
	//shutdown(fd, 2);
	free(sendbuf);
	sendbuf = NULL;
	close(fd);
	//free(pt);
	return 0;
}

static int json_data_creat(struct json_object * data, usb_buf_send * buf) {
    json_object_object_add(data, "bat", json_object_new_string("0"));
	json_object_object_add(data, "sn", json_object_new_string(buf->data.sn));
	json_object_object_add(data, "mac", json_object_new_string(buf->data.mac));
	json_object_object_add(data, "type", json_object_new_string("3"));
	json_object_object_add(data, "rssi", json_object_new_string(buf->data.rssi));
	json_object_object_add(data, "stimestamp", json_object_new_string(buf->data.stim));
	return 0;
}

int SendJsonData(usb_buf_send ** inputList) {
	//form_json_list
	usb_buf_send *rec, *pre = NULL;
	char *pt, *data;
	struct json_object *tempdata = NULL, *array;
	array = json_object_new_array();
	if (NULL == array) {
		LOGE("Error: array json_object_new_array err\n");
		return -1;
	}

	LIST_FOREACH(rec, *inputList) {
		tempdata = json_object_new_object();
		if(NULL == tempdata) {
			LOGE("malloc err");
			json_object_put(array);
			return -1;
		}
		json_data_creat(tempdata, rec);
		json_object_array_add(array, tempdata);
		LIST_REMOVE_HEAD(*inputList);
		if(pre)
			free(pre);
		pre = rec;
		if(tolcnt_g)
			tolcnt_g--;
	}
	if(pre)
		free(pre);
	tolcnt_g = 0;

	if(NULL == array) {
		pre = NULL;
		LIST_FOREACH(rec, *inputList) {
			LIST_REMOVE_HEAD(*inputList);
			if(pre)
				free(pre);
			pre = rec;
		}
		if(pre)
			free(pre);
		tolcnt_g = 0;
		return -1;
	}

	pt = (char *)json_object_to_json_string(array);
	data = (char *)safe_malloc(strlen(pt) + 32);
	if(NULL == data) {
		LOGE("data malloc err");
		json_object_put(array);
		return -2;
	}
	sprintf(data, "list=%s", pt);
	json_object_put(array);
	http_send_to_server(IP, PORT, PATH, data);
	free(data);
	data = NULL;

	return 0;
}
////////<==Make Json & Send Data////////

////////small tools==>////////
static void get_sn(char *buf) {
        FILE *fp = NULL;
        char tmp[64];

        fp = popen("fts getsn", "r");
        if (!fp) {
                exit(1);
                LOGE("get_sn fail!\n"); 
        }

        fgets(tmp, sizeof(tmp), fp);
        if(strlen(tmp)) memcpy(buf,tmp, 16);
        buf[16] = '\0';
        pclose(fp);
}

static void parse_server(char *addr, char* domain, char* port, char* path) {
        char* p1 = NULL, *p2 = NULL, *p3 = NULL;

        p1 = addr;
        if(!strncmp(p1,"http://",7)) {
        	p1 += 7;
        }
        if((p2 = strchr((const char *)p1,':'))) {
            *p2++ = '\0';
			p3 = strchr(p2, '/');
			if(p3) {
				*p3++ = '\0';
			}
        } else {
            p2 = "80";
        	p3 = strchr(p1, '/');
	        if(p3) {
                *p3++ = '\0';
	        }
	}
        if(p1)strcpy(domain,p1);
        if(p2)strcpy(port,p2);
        if(p3)strcpy(path ,p3);
		return;
}

char *reverse_string(char *s) {  
    char temp;  
    char *p = s;
    char *q = s;
    while(*q)  
        ++q;  
    q--;  
  
    while(q > p){  
        temp = *p;  
        *p++ = *q;  
        *q-- = temp;  
    }  
    return s;  
}  
  
char *itoa_dec(int n) { //max.length=15
    int i = 0;  
    static char s[16];
    do {  
        s[i++] = n%10 + '0';  
        n = n/10;  
    } while (n > 0);  
  
    s[i] = '\0';
    return reverse_string(s);    
}  

int itoa_x(int val, char buf[2]) {
	int t = val%16;
	if (t > 9) {  
		buf[1] = t- 10 + 'a';  
	} else {  
		buf[1] = t + '0';  
	}
	val /= 16;
	if (val > 9) {  
		buf[0] = val- 10 + 'a';  
	} else {
		buf[0] = val + '0';  
	}
	return 0;  
} 

int atoi_dec(char* pstr) { // only positive number
	if(pstr == NULL) {  
		LOGE("atoi_dec input is NULL\n");  
		return 0;  
    }  
	int Ret_Integer = 0; 
	while(*pstr >= '0' && *pstr <= '9') {  
		Ret_Integer = Ret_Integer * 10 + *pstr - '0';  
		pstr++;  
	}
	return Ret_Integer;  
}  
////////<==small tools////////

////////MAIN////////
int main(int argc, char **argv) {
	int fd, err, len, length, i; 
	int index = 0, location = 0, finish = 0;
	int stim_int;
	char *stim_char;
	unsigned char rcv_buf[32], print_buf[256];
	char sn_val[17], mac_val[13], rssi_val[3]; 
	char addr[512];
	usb_buf_send *rec, *var, *list_data = NULL;

	USE_SYSLOG(argv[0]);	
	get_sn(sn_val);

	cachesize_g = atoi_dec(argv[4])*1024;
	totalcnt_g = (cachesize_g)/160;
	interval_g = atoi_dec(argv[6]);

	time_t now = time(NULL);
	time_t last_check_time = now;

	fd = UART0_Open("/dev/ttyUSB0");
	if(FALSE == fd) {
		LOGE("open err\n");
		return FALSE;
	}
	sprintf(addr, "%s", argv[2]);
	parse_server(addr, IP, PORT, PATH);

	do {  
		err = UART0_Init(fd,115200,0,8,1,'N');  
		// printf("Set Port Exactly!\n");  
	} while(FALSE == err || FALSE == fd);  
	/*
	if(0 == strcmp(direction,"0"))  
	{  
		for(i = 0;i < 10;i++)  
		{  
			len = UART0_Send(fd,send_buf,10);  
			if(len > 0)  
				printf(" %d send data successful\n",i);  
			else  
				printf("send data failed!\n");  

			sleep(2);  
		}  
		UART0_Close(fd);               
	}  
	else  
	{  */
		while (1)
		{   
			len = UART0_Recv(fd, rcv_buf, sizeof(rcv_buf));
			if(len > 0) {
				for(index=0; index < len-1; index++) {
					if(rcv_buf[index] == 0xa5 && rcv_buf[index+1] == 0xa5){//if end
						finish = 1;
						break;
					}
				}
				// printf("finish = %d\n", finish);

				if(finish == 1) {
					memcpy(&print_buf[location],rcv_buf,index+2);
					location = location + index + 2;
					//check data
					length = print_buf[6];
					if(length + 10 != location) {
						// printf("FAILURE!!\n");
					} else {
						for(i=0; i < 6; i++) {
							// printf("%3.2x",print_buf[i]); 
							itoa_x(print_buf[i],&mac_val[i+i]);
						}
						mac_val[12] = '\0';
						
						itoa_x(print_buf[7+length],rssi_val);
						rssi_val[2]='\0';
						
						stim_int = time(NULL);
						stim_char = itoa_dec(stim_int);

						var = (usb_buf_send *)malloc(sizeof(usb_buf_send));
						sprintf((var->data.sn), sn_val);
						sprintf((var->data.mac), mac_val);
						sprintf((var->data.rssi), rssi_val);
						sprintf((var->data.stim), stim_char);

						if(tolcnt_g < totalcnt_g)
							tolcnt_g++;
						else {
							LIST_FOREACH(rec, list_data) {
								LIST_REMOVE_HEAD(list_data);
								break;
							}
							free(rec);
						}

						LIST_INSERT_TAIL(list_data, rec, var);

						now = time(NULL);
						if((now - last_check_time) > interval_g) {             
							if(tolcnt_g)
								SendJsonData(&list_data);
							last_check_time = now;
						}

					}

					memset(print_buf,0,sizeof(print_buf));
					memcpy(print_buf,&rcv_buf[index+2],len-index-2);//copy the rest data
					location = len-index-2; 
					finish = 0;
				}		
				else {//if not end
					memcpy(&print_buf[location],rcv_buf,len);
					location = location + len;
				}

				if(location > 256){
					// printf("Error: length too long.\n");
					location = 0;//reset location
					continue;
				}
			} else {  
				LOGE("Error: cannot receive data\n");  
			} 
		}              
		UART0_Close(fd);
//	} 
		return 0;
}  