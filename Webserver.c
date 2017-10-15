#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define	QLEN		  32	/* maximum connection queue length	*/
#define	BUFSIZE		4096	// Buffer size for socket communication
#define TRY_FOR_HOMEPAGE 5   //Max tries to read homepage from conf file
#define TIMEOUT 2			//timeout for keep alive connection
 
//struct to store configurations read from the conf file
typedef struct configstruct
{
	char portno[10];
	char rootdirec[100];
	char homepage[100];
	char ext_type[20][100];
	char file_type[20][100];
	char ext_type1[100];

}configstruct_t;

//struct to store parsed http req parameters
typedef struct myerror_struct
{
	char *parse_method;
	char parse_url[100];
	char *parse_protocol;
	char *parse_version;
	char *ext;
	char *adhoc;
}request_t;

// globals
extern int	errno;
char *configfile="ws.conf";
int	msock;
char tempreq[BUFSIZE];
int	ssock;

int		errexit(const char *format, ...);
int		passivesock(const char *portnum, int qlen);

//function to read supported file types from the conf file
int 	content_type(configstruct_t *config_parse)
{
	FILE *fp;
	char configread[200];
	fp=fopen(configfile,"r");

	int i = 0;
	while (!feof(fp)) 
	{
		if(fgets(configread,200,fp)!=NULL)
		{
			if(configread[0]=='#')
				continue;
			//store the supported extensions and file types in the config struct
			else if(configread[0]=='.')
			{
				char *content=strtok((char *)&configread[1]," ");
				sprintf(config_parse->ext_type[i],"%s",content);
				sprintf(config_parse->file_type[i],"%s",strtok(NULL," \n\0"));
				i++;
			}
		}
	}
	return 1;
}

//function that responds to client with error codes (HTTP Error handling)
int errorhandling(int fd, request_t req,configstruct_t *config_parse)
{
	char msg_404[BUFSIZE];
	sprintf(msg_404, "HTTP/%s 404 Not Found\nContent-type: text/html\n\n<html><body>404 Not found Reason URL does not exist: %s </body></html>",req.parse_version,req.parse_url);
	char msg_400_method[BUFSIZE];
	sprintf(msg_400_method, "HTTP/%s 400 Bad Request\nContent-type: text/html\n\n<html><body>400 Bad Request Reason:Invalid Method %s</body></html>",req.parse_version, req.parse_method);
	char msg_400_url[BUFSIZE];
	sprintf(msg_400_url, "HTTP/%s 400 Bad Request\nContent-type: text/html\n\n<html><body>400 Bad Request Reason:Invalid URL %s</body></html>",req.parse_version,req.parse_url);
	char msg_400_version[BUFSIZE];
	sprintf(msg_400_version, "HTTP/%s 400 Bad Request\nContent-type: text/html\n\n<html><body>400 Bad Request Reason:Invalid Version %s</body></html>",req.parse_version,req.parse_version);
	char msg_501_content[BUFSIZE];
	sprintf(msg_501_content, "HTTP/%s 501 Not Implemented\nContent-type: text/html\n\n<html><body>501 Not Implemented : Requested File type (%s) not supported </body></html>",req.parse_version,req.ext);
	
	// Bad method
	if(!(strncmp(req.parse_method,"GET",4)==0)&&!(strncmp(req.parse_method,"POST",5)==0))
	{	
		int s= write(fd,msg_400_method,strlen(msg_400_method));
			return 1;
	}

	// bad version
	if((!(strncmp(req.parse_version,"1.0",3)==0)) && (!(strncmp(req.parse_version,"1.1",3)==0)))
	{	
		int s= write(fd,msg_400_version,strlen(msg_400_version));
			return 1;
	}

	//unsupported file type
	if(!(req.parse_url[strlen(req.parse_url)-1]=='/'))
	{
	int i=0; 
	while(i<20)
	{

		if(strncmp(config_parse->ext_type[i],req.ext,strlen(req.ext))==0)
		{
			break;
		}	
		
		i++;
		if(i==20)
		{
			int s= write(fd,msg_501_content,strlen(msg_501_content));
			return 1;
		}
	}	
}
	
	//URL doesn't exist on server	
	if(strlen(req.parse_url)!=1)
	{
		if(access(req.parse_url,F_OK)!=0)
		{
			int s= write(fd,msg_404,strlen(msg_404));
			return 1;
		}
	}
	else
	{
		close(msock);
		remove(tempreq);
		exit(0);
	}

return 0;
}

// function to handle CTRL+C and exit gracefully
void signalHandler(int sig_num)
{
	char msg_500[BUFSIZE];
	sprintf(msg_500, "HTTP/1.1 500 Internal Server Error\nContent-type: text/html\n\n<html><body>500 Internal Server Error</body></html>");
	int s= write(ssock,msg_500,strlen(msg_500));
	close(ssock);
	close(msock);
	remove(tempreq);
	exit(0);
}

//function to read port number from conf file
int readport()
{
	FILE *fp;
	char configread[100];
	fp=fopen(configfile,"r");
	while (!feof(fp)) 
	{
		if(fgets(configread,100,fp)!=NULL)
		{
			if(configread[0]=='#')
				continue;
			else
			{
				char *portno=strtok(configread," ");
				if(strncmp(portno,"Listen",6)==0)
				{
				   portno=strtok(NULL,"\n");
				   return atoi(portno);
				}
				else
				   continue;
			}
		}
	}
	return -1;
}

//function to read the server's directory root from the conf file
int serverroot(configstruct_t *config_parse)
{
	FILE *fp;
	char configread[200];
	fp=fopen(configfile,"r");
	while (!feof(fp)) 
	{
		if(fgets(configread,200,fp)!=NULL)
		{
			if(configread[0]=='#')
				continue;
			else
			{
				char *rootdirec=strtok(configread,"\"");
				if(strncmp(rootdirec,"DocumentRoot",12)==0)
				{
				   rootdirec=strtok(NULL,"\"");
				   sprintf(config_parse->rootdirec,"%s",rootdirec);
				   return 1;
				}
				else
				   continue;
			}
		}
	}
	return -1;
}

//function to find out the homepage from the conf file
int homepage(configstruct_t *config_parse)
{
	FILE *fp;
	char configread[200];
	char tp[20];
	fp=fopen(configfile,"r");
	while (!feof(fp)) 
	{
		if(fgets(configread,200,fp)!=NULL)
		{
			if(configread[0]=='#')
				continue;
			else
			{
				char *webpage=strtok(configread," ");
				char webpage_temp[100];
				if(strncmp(webpage,"DirectoryIndex",strlen("DirectoryIndex"))==0)
				{
					int i=0;
					int j=0;
					while(i<TRY_FOR_HOMEPAGE)
					{
						webpage=strtok(NULL," \n");
						sprintf(webpage_temp,"%s/%s",config_parse->rootdirec,webpage);
						if(access(webpage_temp,F_OK)==0)
						{
							sprintf(config_parse->homepage,"%s",webpage_temp);
							break;
						}
						i++;
					}
					//figure out the type of the homepage
					char *temp2 = strtok(webpage,".");
					sprintf(tp,"%s",strtok(NULL," \n\0"));
					while(j<20)
					{
						if(strncmp(config_parse->ext_type[j],tp,strlen(tp))==0)
						{
							sprintf(config_parse->ext_type1,"%s",config_parse->file_type[j]);
							return 1;
						}
						j++;
					}
					return 0;
				}
				else
				   continue;
			}
		}
	}
	return -1;
}

//function that handles HTTP req
int webserver(int fd,configstruct_t *config_parse)//considering forking for every such new request
{
	int pipeline=0;
	int cc=0;
	
	char req_read[BUFSIZE];
	fcntl(fd, F_SETFL, O_NONBLOCK);		//create a non blocking read
	FILE *readreq;
	FILE *writereq;
	sprintf(tempreq,"temp_%d",getpid());
	//Pipelining and keep alive
	while(pipeline<TIMEOUT)
	{	
		int charcount= 0;
		int readcount= 0;
		memset(req_read,0,BUFSIZE);
		int r = 0;
		if(pipeline==0)
		{
			writereq=fopen(tempreq,"w");						//start writing incoming requests from browser
			//store the HTTP req in a temp file
			while(readcount<BUFSIZE)
			{
				readcount++;
				if(read(fd,&req_read[charcount],1)>0)
				{	
					fprintf(writereq,"%c",req_read[charcount]);
					charcount++;
				}
				else
				{
					break;
				}	
			}
			fclose(writereq);
		}
		readreq=fopen(tempreq,"r");
		//read from file and serve all requests in case of pipelining
		TAG:
		
			cc=0;
			memset(req_read,0,BUFSIZE);
			fgets(req_read,BUFSIZE,readreq);
			if(strncmp(req_read,"GET",4)||strncmp(req_read,"POST",5))
			{
				cc = strlen(req_read);
				char header[BUFSIZE];
				while(fgets(header,BUFSIZE,readreq))
				{
      				if((header[0]=='\n')||feof(readreq)||(header[0]=='\r'))
      					break;
  				}
  				if(pipeline > 0 && pipeline < TIMEOUT)
  				{
  					pipeline++;
  					goto TAG;
  				}

  						
  		    }
	fclose(readreq);


	if(cc>1) 
	{
		request_t req;
		char *parse_method= strtok(req_read," "); 
		char *parse_url= strtok(NULL," ");
		char *parse_protocol=strtok(NULL,"/");
		char *parse_version=strtok(NULL,"\n\0 ");
		sprintf(req.parse_url,"%s%s",config_parse->rootdirec,parse_url);
		char *filename_ext_less=strtok(parse_url,".");
		char *ext=strtok(NULL,"\0\n ");
		
		req.parse_method=parse_method;
		req.parse_protocol=parse_protocol;
		req.parse_version=parse_version;
		req.ext=ext;

		printf("method %s\n",parse_method);
		printf("url %s\n",req.parse_url);
		printf("protocol %s\n",parse_protocol);
		printf("version %s\n\n",parse_version);
		
		//for invalid requests
		if(errorhandling(fd,req,config_parse))
		{
			close(fd);
			remove(tempreq);
			return cc;	
		}
		else
		{	
			//for valid requests
			int i=0;
			int j=0;
			//if no URL is specified then send homepage
			if(strlen(parse_url)==1)
			{
				FILE *fp;
				fp=fopen(config_parse->homepage,"r");
				char filebuf[BUFSIZE];
				int bytes_read;
				fseek(fp,0,SEEK_END);			
				int filesize=ftell(fp);
				fseek(fp,0,SEEK_SET);
				char msg_200[BUFSIZE];
				if(strlen(req.parse_method) > 3)
				{
					//handle POST
					sprintf(msg_200,"HTTP/%s 200 OK\nContent-type: %s\nContent-size:%d\n\n<html><body><pre><h1>POST is working</h1></pre>",req.parse_version,config_parse->ext_type1,filesize);
				}
				else
				{
					sprintf(msg_200,"HTTP/%s 200 OK\nContent-type: %s\nContent-size:%d\n\n",req.parse_version,config_parse->ext_type1,filesize);
				}
				
				write(fd,msg_200,strlen(msg_200));
				bytes_read=fread(filebuf,1,1000,fp);
				while(bytes_read>0)
				{
					int s1=write(fd,filebuf,bytes_read);
					bytes_read=fread(filebuf,1,1000,fp);
				}
				fclose(fp);
			}
			else
			{
				//for specific URL
				FILE *fp;
				fp=fopen(req.parse_url,"r");
				fseek(fp,0,SEEK_END);
				int filesize=ftell(fp);
				fseek(fp,0,SEEK_SET);
				char filebuf[BUFSIZE];
				int bytes_read;
				char msg_200[BUFSIZE];
				//figure out the type of the file
			while(i<20)
			{
				if(strncmp(config_parse->ext_type[i],req.ext,strlen(req.ext))==0)
				{
					break;
				}
				i++;
			}
			if(strlen(req.parse_method) > 3)
			{
				//handle POST
				sprintf(msg_200,"HTTP/%s 200 OK\nContent-type: %s\nContent-size:%d\n\nPOST\n",req.parse_version,config_parse->file_type[i],filesize);
			}
			else
			{
			sprintf(msg_200,"HTTP/%s 200 OK\nContent-type: %s\nContent-size:%d\n\n",req.parse_version,config_parse->file_type[i],filesize);
		}
			write(fd,msg_200,strlen(msg_200));
			
			bytes_read=fread(filebuf,1,1000,fp);
			
			while(bytes_read>0)
			{
				int s1=write(fd,filebuf,bytes_read);
				bytes_read=fread(filebuf,1,1000,fp);
			}
			fclose(fp);
		}
			
		}

	}
	pipeline++;
	sleep(1);	
}
	close(fd);
	remove(tempreq);
	return cc;		
}
/*------------------------------------------------------------------------
 * passivesock - allocate & bind a server socket using TCP
 *------------------------------------------------------------------------
 */
int
passivesock(const char *portnum, int qlen)
/*
 * Arguments:
 *      portnum   - port number of the server
 *      qlen      - maximum server request queue length
 */
{
        struct sockaddr_in sin; /* an Internet endpoint address  */
        int     s;              /* socket descriptor             */

        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;

    /* Map port number (char string) to port number (int) */
        if ((sin.sin_port=htons((unsigned short)atoi(portnum))) == 0)
        {
                errexit("can't get \"%s\" port number\n", portnum);	
        }

    /* Allocate a socket */
        s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s < 0)
            errexit("can't create socket: %s\n", strerror(errno));

    /* Bind the socket */
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) 
		{
            fprintf(stderr, "can't bind to %s port: %s",
                portnum, strerror(errno));
            sin.sin_port=htons(0); /* rfequest a port number to be allocated
                                   by bind */
            if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
                errexit("can't bind: %s\n", strerror(errno));
            else {
                int socklen = sizeof(sin);

                if (getsockname(s, (struct sockaddr *)&sin, &socklen) < 0)
                        errexit("getsockname: %s\n", strerror(errno));
                }
        }

        if (listen(s, qlen) < 0)
            errexit("can't listen on %s port: %s\n", portnum, strerror(errno));
        return s;
}

/*------------------------------------------------------------------------
 * errexit - print an error message and exit
 *------------------------------------------------------------------------
 */
int
errexit(const char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
}

//wrapper function to read all the configurations from the conf file
int readconf(configstruct_t *config_parse)
{
	
	int i=0;
	if(access(configfile,F_OK)!=0)
	{
			printf("Configuration file not found\n");
			exit(0);
		
	}
	int portno_temp = readport();
	sprintf(config_parse->portno,"%d",portno_temp);
	serverroot(config_parse);
	content_type(config_parse);
	homepage(config_parse);
	return 1;
}

int main(int argc, char *argv[])
{
	configstruct_t *config_parse = (configstruct_t *)malloc(10000);
	readconf(config_parse);

	char portnum[20];
	sprintf(portnum,"%d",readport());/* Standard server port number	*/
	signal(SIGINT,signalHandler);//whenever u reacive interrupt pls close socportno %s\n	
	signal(SIGQUIT,signalHandler);
	signal(SIGKILL,signalHandler);
	signal(SIGTERM,signalHandler);
	
	struct sockaddr_in fsin;	/* the from address of a client	*/
				/* master server socket		*/
	unsigned int alen;		/* from-address length		*/
	msock = passivesock(portnum, QLEN);


	while (1) 
	{

			alen = sizeof(fsin);
			ssock = accept(msock, (struct sockaddr *)&fsin,&alen);
			if (ssock < 0)
				errexit("accept: %s\n",strerror(errno));
			
			if (ssock>0)
			{
				int p=fork();
				if (p==0)
				{
					//child process to handle individual clients/req
					close(msock);
					webserver(ssock,config_parse);
					close(ssock);
					exit(0);
				}
				else
				{
					//parent goes back to listening on master sock
					close(ssock);
				}

			}
	}
}



