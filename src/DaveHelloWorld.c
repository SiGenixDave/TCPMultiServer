#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#define closesocket close
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <sys/types.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE   1
#define FALSE  0
#define PORT1 8888
#define PORT2 9999

#define MAX_CLIENTS  				100
#define MAX_SERVER_SOCKETS			20

static int mClientSocket[MAX_CLIENTS];
static int mNumServerSockets;
static int mServerSocket[MAX_SERVER_SOCKETS];
static struct sockaddr_in mServerAddressInfo[MAX_SERVER_SOCKETS];
static fd_set mReadfds; //set of socket descriptors


static void ccInitTcpConnections (void);
static int ccPopulateSocketDescriptorList (void);
static void ccCreateServerSocket (unsigned aPort);
static void ccScanForNewConnections (void);

int main(int argc , char *argv[])
{

    int activity, i, valread, maxSd, sd;
    int addrlen;
    struct sockaddr_in addressInfo;

    char buffer[1025];  //data buffer of 1K



    struct timeval timer;

    timer.tv_sec = 0;
    timer.tv_usec = 0;

    /*********************************/
    // Used exclusively for winsock
    WSADATA wsaData;   // if this doesn't work
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }
    /**********************************/

    ccInitTcpConnections();

    ////////////////////////////////////////////////////////////////////////////////////////////
    ccCreateServerSocket (8888);
    ccCreateServerSocket (9999);



    //accept the incoming connection
    puts("Waiting for connections ...");

    while(TRUE)
    {

    	maxSd = ccPopulateSocketDescriptorList ();


        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( maxSd + 1 , &mReadfds , NULL , NULL , NULL);
        //activity = select( max_sd2 + 1 , &mReadfds , NULL , NULL , &timer);

        if ( (activity < 0) && (errno!=EINTR) )
        {
            printf("select error");
        }


        ccScanForNewConnections();

        // its some IO operation on some other socket :)
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            sd = mClientSocket[i];

            if (FD_ISSET( sd , &mReadfds))
            {
            	valread = recv( sd , (void *)buffer, 1024, 0);
                //Check if it was for closing , and also read the incoming message
                if (valread == 0)
                {
                	// TODO
                	addrlen = sizeof(addressInfo);
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr *)&addressInfo , (socklen_t *)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(addressInfo.sin_addr) , ntohs(addressInfo.sin_port));

                    // Close the socket and mark as 0 in list for reuse
                    shutdown( sd, 2 );
                    mClientSocket[i] = 0;
                }

                //Echo back the message that came in
                else if (valread > 0)
                {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    send(sd , buffer , strlen(buffer) , 0 );
                }
            }
        }
    }

    return 0;
}


static void ccInitTcpConnections( void )
{
	int i;

	mNumServerSockets = 0;

    //Initialize all mClientSocket[] to 0 so not checked
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        mClientSocket[i] = 0;
    }

}


static int ccPopulateSocketDescriptorList (void)
{
	int maxSd;
	int sd;
	int i;

    /////////////////////////////////////////////////////////////////////////////////////////////////
	// All of this is required each time in the while loop because the select function modifies the
	// flags in the file
	// descriptors
    //clear the socket set
    FD_ZERO(&mReadfds);

    //add master socket to set
    FD_SET(mServerSocket[0], &mReadfds);
    FD_SET(mServerSocket[1], &mReadfds);
    maxSd = mServerSocket[1];


    //add child sockets to set
    for ( i = 0 ; i < MAX_CLIENTS ; i++)
    {
        //socket descriptor
        sd = mClientSocket[i];

        //if valid socket descriptor then add to read list
        if(sd > 0)
        {
            FD_SET( sd , &mReadfds);
        }

        //highest file descriptor number, need it for the select function
        if(sd > maxSd)
        	maxSd = sd;

    }
    /////////////////////////////////////////////////////////////////////////////////////////////////

    return maxSd;
}

static void ccCreateServerSocket (unsigned aPort)
{
	int serverSocket;
	struct sockaddr_in address;
    int opt = TRUE;

	serverSocket = socket(AF_INET , SOCK_STREAM , 0);

    //create a master socket
    if (serverSocket == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( aPort );

    //bind the socket to local host port 8888
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", aPort);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(serverSocket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    mServerAddressInfo[mNumServerSockets] = address;
    mServerSocket[mNumServerSockets] = serverSocket;
    mNumServerSockets++;

}

static void ccScanForNewConnections (void)
{

    //a message
    char *message[] = {
    				  	  "ECHO Daemon v1.0 server port 8888 \r\n",
    				  	  "ECHO Daemon v1.0 server port 9999 \r\n"
					  };

    int new_socket;

    int socketId = 0;
    int addrlen;
    int i;

    while (mServerSocket[socketId] != 0)
    {
        addrlen = sizeof(mServerAddressInfo[socketId]);

		//If something happened on the master socket , then its an incoming connection
		if (FD_ISSET(mServerSocket[socketId], &mReadfds) != 0)
		{
			if ((new_socket = accept(mServerSocket[socketId], (struct sockaddr *)&mServerAddressInfo[socketId], (socklen_t*)&addrlen))<0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			//inform user of socket number - used in send and receive commands
			printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(mServerAddressInfo[socketId].sin_addr) , ntohs(mServerAddressInfo[socketId].sin_port));

			//send new connection greeting message
			if( send(new_socket, message[socketId], strlen(message[socketId]), 0) != strlen(message[socketId]) )
			{
				perror("send");
			}

			puts("Welcome message sent successfully");

			//add new socket to array of sockets
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				//if position is empty
				if( mClientSocket[i] == 0 )
				{
					mClientSocket[i] = new_socket;
					printf("Adding to list of sockets as %d\n" , i);

					break;
				}
			}
		}
		socketId++;

    }

}
