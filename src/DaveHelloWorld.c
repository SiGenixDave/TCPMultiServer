// Override default setting of 64 in winsock2.h; max # of total TCP sockets
// (client and server) that can be handled by select ()
#define FD_SETSIZE	512


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

///////////////////////////////////////////////////////////////////////////////////////
#define TRUE   1
#define FALSE  0
#define PORT1 8888
#define PORT2 9999

#define MAX_CLIENTS_PER_SERVER		10
#define MAX_SERVER_SOCKETS			20

///////////////////////////////////////////////////////////////////////////////////////
typedef void ccServerCallbackFunc(char *aBuffer, int aNumBytes, int aClientSocketId);

typedef struct
{
	unsigned port;
	struct sockaddr_in addressInfo;
	ccServerCallbackFunc *callbackFunc;
	int socketId;
	int clientSockets[MAX_CLIENTS_PER_SERVER];
} ServerSocketInfo;

///////////////////////////////////////////////////////////////////////////////////////
static int mNumServerSockets;
static ServerSocketInfo mServers[MAX_SERVER_SOCKETS];
static fd_set mReadfds; //set of socket descriptors
static struct timeval mTimer;
static struct timeval *mTimerPtr;


///////////////////////////////////////////////////////////////////////////////////////
static void ccInitTcpConnections (void);
static void ccSetBlockingTime (long int aSeconds, long int aMicroSeconds);
static void ccBlockUntilActivity (void);
static void ccServiceIncomingSocketData (void);
static int ccPopulateSocketDescriptorList (void);
static void ccCreateServerSocket (unsigned aPort, ccServerCallbackFunc aCallBackFunc);
static void ccScanForNewConnections (void);


void ccServerCallback8888 (char *aBuffer, int aNumBytes, int aClientSocketId)
{
	char str[200];

	sprintf(str, "Port 8888 --- SocketID = %d   ---- Msg = ", aClientSocketId);

	//set the string terminating NULL byte on the end of the data read
	aBuffer[aNumBytes] = '\0';
	strcat(str, aBuffer);
	strcat(str, "\n");

	send(aClientSocketId , str , strlen(str) , 0 );
	printf("Server 8888: SocketId = %d, # Bytes in Msg = %d, Msg = %s", aClientSocketId, aNumBytes, aBuffer);

}

void ccServerCallback9999 (char *aBuffer, int aNumBytes, int aClientSocketId)
{
	char str[200];

	sprintf(str, "Port 9999 --- SocketID = %d   ---- Msg = ", aClientSocketId);

	//set the string terminating NULL byte on the end of the data read
	aBuffer[aNumBytes] = '\0';
	strcat(str, aBuffer);
	strcat(str, "\n");

	send(aClientSocketId , str , strlen(str) , 0 );
	printf("Server 9999: SocketId = %d, # Bytes in Msg = %d, Msg = %s", aClientSocketId, aNumBytes, aBuffer);
}

int main(int argc , char *argv[])
{

    int activity, maxSd;

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
    ccCreateServerSocket (8888, ccServerCallback8888);
    ccCreateServerSocket (9999, ccServerCallback9999);

    ccBlockUntilActivity();


    //accept the incoming connection
    puts("Waiting for connections ...");

    while(TRUE)
    {

    	maxSd = ccPopulateSocketDescriptorList ();


        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select (maxSd + 1, &mReadfds, NULL, NULL, mTimerPtr);

        if ( (activity < 0) && (errno!=EINTR) )
        {
            printf("select error");
        }


        ccScanForNewConnections ();

        ccServiceIncomingSocketData ();

    }

    return 0;
}


static void ccInitTcpConnections( void )
{
	mNumServerSockets = 0;

    //TODO Initialize all mClientSocket[] to 0 so not checked

	ccSetBlockingTime (0, 0);

}


static void ccSetBlockingTime (long int aSeconds, long int aMicroSeconds)
{
	mTimer.tv_sec = aSeconds;
	mTimer.tv_usec = aMicroSeconds;

	mTimerPtr = &mTimer;
}

static void ccBlockUntilActivity (void)
{
	mTimerPtr = NULL;
}

static int ccPopulateSocketDescriptorList (void)
{
	int maxSd;
	int sd;
	int i;
	int socketCnt = 0;

	// All of this is required each time in the while loop because the select function modifies the
	// flags in the file descriptors
    // clear the socket set
    FD_ZERO(&mReadfds);

    maxSd = 0;
    while (mServers[socketCnt].socketId != 0)
    {
    	//add master socket to set
    	FD_SET(mServers[socketCnt].socketId, &mReadfds);
    	if (mServers[socketCnt].socketId > maxSd)
    	{
    		maxSd = mServers[socketCnt].socketId;
    	}
    	socketCnt++;
    }

    socketCnt = 0;
    while (mServers[socketCnt].socketId != 0)
    {
    	//add child sockets to set
		for ( i = 0 ; i < MAX_CLIENTS_PER_SERVER ; i++)
		{
			//socket descriptor
			sd = mServers[socketCnt].clientSockets[i];

			//if valid socket descriptor then add to read list
			if(sd > 0)
			{
				FD_SET( sd , &mReadfds);
			}
			else
			{
				break;
			}

			//highest file descriptor number, need it for the select function
			if(sd > maxSd)
			{
				maxSd = sd;
			}
		}

		socketCnt++;
	}

    return maxSd;
}


static void ccServiceIncomingSocketData (void)
{
	int i, sd, valread;
    int addrlen;
    struct sockaddr_in addressInfo;
	char buffer[1024];
	int socketCnt = 0;

	while (mServers[socketCnt].socketId != 0)
	{

		// its some IO operation on some other socket :)
		for (i = 0; i < MAX_CLIENTS_PER_SERVER; i++)
		{
			sd = mServers[socketCnt].clientSockets[i];

			if (sd == 0)
			{
				break;
			}

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
					mServers[socketCnt].clientSockets[i] = 0;
				}

				//Echo back the message that came in
				else if (valread > 0)
				{
					mServers[socketCnt].callbackFunc(buffer, valread, sd);
				}
			}
		}
		socketCnt++;
	}

}

static void ccCreateServerSocket (unsigned aPort, ccServerCallbackFunc aCallbackFunc)
{
	int serverSocket;
	struct sockaddr_in address;
    int opt = TRUE;

	serverSocket = socket(AF_INET , SOCK_STREAM , 0);

    //create a master socketId
    if (serverSocket == 0)
    {
        perror("socketId failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socketId created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons (aPort);

    //bind the socket to local host port 8888
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", aPort);

    //try to specify maximum of 3 pending connections for the master socketId
    if (listen(serverSocket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    mServers[mNumServerSockets].port = aPort;
    mServers[mNumServerSockets].addressInfo = address;
    mServers[mNumServerSockets].socketId = serverSocket;
    mServers[mNumServerSockets].callbackFunc = aCallbackFunc;
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

    int socketCnt = 0;
    int addrlen;
    int i;

    while (mServers[socketCnt].socketId != 0)
    {
        addrlen = sizeof(mServers[socketCnt].addressInfo);

		//If something happened on the master socketId , then its an incoming connection
		if (FD_ISSET(mServers[socketCnt].socketId, &mReadfds) != 0)
		{
			new_socket = accept(mServers[socketCnt].socketId, (struct sockaddr *)&mServers[socketCnt].addressInfo, (socklen_t*)&addrlen);
			if (new_socket < 0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			//inform user of socketId number - used in send and receive commands
			printf("New connection , socketId fd is %d , ip is : %s , port : %d \n",
					new_socket,
					inet_ntoa(mServers[socketCnt].addressInfo.sin_addr),
					ntohs(mServers[socketCnt].addressInfo.sin_port));

			//send new connection greeting message
			if( send(new_socket, message[socketCnt], strlen(message[socketCnt]), 0) != strlen(message[socketCnt]) )
			{
				perror("send");
			}

			printf("Welcome message sent successfully\n");

			//add new socketId to array of sockets
			for (i = 0; i < MAX_CLIENTS_PER_SERVER; i++)
			{
				//if position is empty
				if( mServers[socketCnt].clientSockets[i] == 0 )
				{
					mServers[socketCnt].clientSockets[i] = new_socket;
					printf("Adding to list of sockets as %d\n" , i);

					break;
				}
			}
		}
		socketCnt++;

    }

}
