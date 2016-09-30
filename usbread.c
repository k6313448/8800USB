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

#define FALSE  -1  
#define TRUE   0  
//#define IP "172.16.0.223"
#define IP "121.40.90.86"
#define PORT "8000"
#define PATH "qcloud/tanzheng/api.php?action=reportingData"

int UART0_Open(char* port)  
{  
	int fd;

	fd = open( port, O_RDWR|O_NOCTTY|O_NDELAY);  
	if (FALSE == fd)  
	{  
		perror("Can't Open Serial Port");  
		return(FALSE);  
	}  

	if(fcntl(fd, F_SETFL, 0) < 0)  
	{  
		printf("fcntl failed!\n");  
		return(FALSE);  
	}       
	else  
	{  
		printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));  
	}  

	if(0 == isatty(STDIN_FILENO))  
	{  
		printf("standard input is not a terminal device\n");  
		return(FALSE);  
	}  
	else  
	{  
		printf("isatty success!\n");  
	}                
	printf("fd->open=%d\n",fd);  
	return fd;  
}  


void UART0_Close(int fd)  
{  
	close(fd);  
}  

int UART0_Set(int fd,int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  
	int   i;  
	//int   status;  
	int   speed_arr[] = { B115200, B19200, B9600, B4800, B2400, B1200, B300};  
	int   name_arr[] = {115200,  19200,  9600,  4800,  2400,  1200,  300};  

	struct termios options;  

	/*tcgetattr(fd,&options)*/
	if  ( tcgetattr( fd,&options)  !=  0)  
	{  
		perror("SetupSerial 1");      
		return(FALSE);   
	}  

	for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)  
	{  
		if  (speed == name_arr[i])  
		{               
			cfsetispeed(&options, speed_arr[i]);   
			cfsetospeed(&options, speed_arr[i]);    
		}  
	}       

	options.c_cflag |= CLOCAL;  
	options.c_cflag |= CREAD;  

	switch(flow_ctrl)  
	{  

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
	switch (databits)  
	{    
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

	switch (parity)  
	{    
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

	switch (stopbits)  
	{    
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

	if (tcsetattr(fd,TCSANOW,&options) != 0)    
	{  
		perror("com set error!\n");    
		return (FALSE);   
	}  
	return (TRUE);   
}  

int UART0_Init(int fd, int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  
	//int err;  
	if (UART0_Set(fd,115200,0,8,1,'N') == FALSE)  
	{                                                           
		return FALSE;  
	}  
	else  
	{  
		return  TRUE;  
	}  
}  


int UART0_Recv(int fd, char *rcv_buf,int data_len)  
{  
	int len,fs_sel;  
	fd_set fs_read;  

	struct timeval time;  

	FD_ZERO(&fs_read);  
	FD_SET(fd,&fs_read);  

	time.tv_sec = 10;
	time.tv_usec = 0;

	fs_sel = select(fd+1,&fs_read,NULL,NULL,&time);  
	if(fs_sel)  
	{  
		len = read(fd,rcv_buf,data_len);  
		printf("Recv Success! len = %d fs_sel = %d\n",len,fs_sel);  
		return len;  
	}  
	else  
	{  
		printf("Recv Fail!\n");  
		return FALSE;  
	}       
}  

int UART0_Send(int fd, char *send_buf,int data_len)  
{  
	int len = 0;  

	len = write(fd,send_buf,data_len);  
	if (len == data_len )  
	{  
		return len;  
	}       
	else     
	{  
		tcflush(fd,TCOFLUSH);  
		return FALSE;  
	}  

}  

static int create_and_connect(const char *addr, const char *port, int type, struct sockaddr *ser)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, listen_sock;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = type; /* We want a TCP socket */

	s = getaddrinfo(addr, port, &hints, &result);
	if (s != 0) {
		//LOGE("getaddrinfo: %s", gai_strerror(s));
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
				printf("socket connect success.\n");
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
		//LOGE("Could not connect to server");
		return -1;
	}

	freeaddrinfo(result);

	return listen_sock;
}

char* build_http_header(char* ip,char* port, char* path, char * data, int len)
{
	//time_t now;
	//char timebuf[100];
	char head[128];
	//now = time(0);
	//strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );

	char *http_header = NULL;
	http_header = malloc(512 + len);

	if(!strcmp(port, "80")) {
		snprintf(head, sizeof(head), "POST http://%s/%s HTTP/1.1\r\n", ip, path);
	} else {
		snprintf(head, sizeof(head), "POST http://%s:%s/%s HTTP/1.1\r\n", ip, port, path);
	}
	
	snprintf(http_header, 512 + len,"%s\
Accept: */*\r\n\
Accept-Language: zh-cn\r\n\
User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n\
Host: %s\r\n\
Referer: http://%s\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Pragma: no-cache\r\n\
Connection: Keep-Alive\r\n\
Content-Length: %d\r\n\r\n%s\n",head,ip,ip,len,data);

	return http_header;
}

static int http_send_to_server(char *ip, char *port, char *path, char *buf)
{
	int fd, r, len;
	int cnt = 0;
	char *sendbuf;
	char *recvbuf;
	len = strlen(buf);
	sendbuf = build_http_header(ip, port, path, buf, len);
	len = strlen(sendbuf);
	printf("Final Data: \n%s\n", sendbuf);
	//if(verbose) LOGI("send data:%s", sendbuf);
	fd = create_and_connect(ip, port, SOCK_STREAM, NULL);
	//r = send(fd, sendbuf, len, 0);
	while (1) {
		r = send(fd, sendbuf,len, 0);
		printf("r = %d\n",r);
		if(r < 0)
		{
			printf("Send Failed!\n");
			break;
			// if (errno == EINTR)
			// {
			// 	continue;
			// } else {
			// 	printf("Send Failed!\n");
			// 	if(++cnt > 3) {
			// 		break;
			// 	} else {
			// 		close(fd);
			// 		fd = create_and_connect(ip,port, SOCK_STREAM, NULL);
			// 		continue;
			// 	}
			// }
		}
		else if(r > 0 && r < len)
		{
			printf("Send partly!\n");
			sendbuf += r;
			len -= r;
			continue;
		}
		else
		{
			printf("Send Finished!\n");
			break;
		}
	}
	//shutdown(fd, 2);
	close(fd);
	//free(pt);
	return 0;
}

static void get_sn(char *buf)
{
        FILE *fp = NULL;
        char tmp[64];

        fp = popen("fts getsn", "r");
        if (!fp) {
                //LOGE("bt scan popen failed");
                //exit(EXIT_FAILURE);
                printf("get_sn fail!\n"); 
        }

        fgets(tmp, sizeof(tmp), fp);
        if(strlen(tmp)) memcpy(buf,tmp, 16);
        buf[16] = '\0';
        pclose(fp);
}

int itoa_x(int val, char buf[2])
{
	int t;
	t = val%16;
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

char* form_json_data(char *sn, char *mac, char *rssi)
{
	char *data_json = NULL;
	data_json = malloc(512);
	int j;
	j = sprintf(data_json,"list=[ { \"bat\": \"0\", \"sn\": \"");
	j += sprintf(data_json+j,"%s",sn);
	j += sprintf(data_json+j,"\", \"mac\": \"");
	j += sprintf(data_json+j,"%s",mac);
	j += sprintf(data_json+j,"\", \"type\": \"3\", \"rssi\": \"");
	j += sprintf(data_json+j,"%s",rssi); 
	j += sprintf(data_json+j,"\", \"stimestamp\": \"1499999999\" } ]");
	return data_json;
}

int main(int argc, char **argv)  
{  
	int fd; 
	int err;
	int len;   
	int length;
	int index = 0;
	int location = 0;
	int finish = 0;
	int i;
	unsigned char rcv_buf[32];         
	unsigned char print_buf[256];
	char *data_json = NULL;
	data_json = calloc(1,512); 
	char sn_val[17];
	char mac_val[13];
	char rssi_val[3]; 
	char send_buf[20]="tiger john";  
	get_sn(sn_val);
	if(argc != 3)  
	{  
		printf("Usage: %s /dev/ttySn 0(send data)/1 (receive data) \n",argv[0]);  
		return FALSE;  
	}  
	fd = UART0_Open(argv[1]);
	if(FALSE == fd) {
		printf("open err\n");
		return FALSE;
	}
	do{  
		err = UART0_Init(fd,115200,0,8,1,'N');  
		printf("Set Port Exactly!\n");  
	}while(FALSE == err || FALSE == fd);  

	if(0 == strcmp(argv[2],"0"))  
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
	{  
		while (1)
		{   
			len = UART0_Recv(fd, rcv_buf, sizeof(rcv_buf));
			if(len > 0){
				for(index=0; index < len-1; index++){
					if(rcv_buf[index] == 0xa5 && rcv_buf[index+1] == 0xa5){//if end
						finish = 1;
						break;
					}
				}
				printf("finish = %d\n", finish);

				if(finish == 1)
				{
					memcpy(&print_buf[location],rcv_buf,index+2);
					location = location + index + 2;
					//check data
					length = print_buf[6];
					if(length + 10 != location)
					{
						printf("FAILURE!!\n");
					}
					else
					{
						printf("SUCCESS!!\n");
						//print data
						printf("location = %d\n", location);
						for(i=0; i < location; i++){
							printf("%3.2x",print_buf[i]);  
						}
						printf("\n");

						printf("mac: ");
						for(i=0; i < 6; i++) {
							printf("%3.2x",print_buf[i]); 
							itoa_x(print_buf[i],&mac_val[i+i]);
						}
						mac_val[12] = '\0';

						printf("\n");
						printf("length: ");
						printf("%3.2x",print_buf[6]);  
						printf("\n");
						
						printf("data: ");
						for(i=7;i < 7+length; i++) {
							printf("%3.2x",print_buf[i]);  
						}
						printf("\n");
						printf("rssi:");
						printf("%3.2x",print_buf[7+length]); 
						itoa_x(print_buf[7+length],rssi_val);
						rssi_val[2]='\0';
						printf("\n");

						data_json=form_json_data(sn_val, mac_val, rssi_val);
						printf("data_json: %s\n", data_json);
						http_send_to_server(IP, PORT, PATH, data_json);
						memset(data_json,0,sizeof(data_json));
					}

					memset(print_buf,0,sizeof(print_buf));
					memcpy(print_buf,&rcv_buf[index+2],len-index-2);//copy the rest data
					location = len-index-2; 
					finish = 0;
				}		
				else{//if not end
					memcpy(&print_buf[location],rcv_buf,len);
					location = location + len;
				}

				if(location > 256){
					printf("Error: length too long.\n");
					location = 0;//reset location
					continue;
				}
				printf("\n");
			}  
			else  
			{  
				printf("Error: cannot receive data\n");  
			} 
		}              
		UART0_Close(fd);   
	} 
}  