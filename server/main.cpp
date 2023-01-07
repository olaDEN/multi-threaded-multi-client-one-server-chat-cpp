#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <vector>
#include <functional> //for std::function
#include <mutex>

using namespace std;

#define IP_ADDRESS "192.168.43.125" //Server socket ip (local for now)
#define DEFAULT_PORT "5555" //listening port
#define DEFAULT_BUFLEN 32768
const char OPTION_VALUE = 1; //option for setsockopt OPTION_VALUE, 1 = enabled
const int MAX_CLIENTS = 5; //Max number of clienst to be accepted per server run
string receivedMessage = ""; //Global variable to hold the received message from client
string senderName = "";//Global variable to hold the sender cleint's name.
int last_active_client = -1;// keep track of the index of the last active client
string client_Inf;// Formatting the client list before saving it into clientsInfo[DEFAULT_BUFLEN] array and send it to client.
int k=0;

//client structure that'll be set in the main and used in the thread.
struct client_type
{
    int id;
    SOCKET socket;
    string IP[512];
    string name[512];
};


//called by corruptMSGrandom to jenerate a random string
string random_string( size_t length )
{
    auto randchar = []() -> char
    {
        const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    string str(length,0);
    generate_n( str.begin(), length, randchar );
    return str;
}

//Called when client send a message using MESG command, it returns either a true or corrupted message to be sent to the receiver.
string corruptMSGrandom(string receivedMessage){

    string request;
    size_t length = static_cast< size_t>(receivedMessage.length());
    string corruption = random_string(length); //Generate random string
    string trueMSGwParity;
    string errMSGwParity;

    cout<<receivedMessage<<endl;

    //MESSAG FORMAT: parity|message|check; if (parity = check) in the message arrived to receiver, message is true, if (parity != check), it is error
    if (receivedMessage.compare(0, 1, "0") == 0){ //take the first bit of the receivedMessage message.
        cout<<"EVEN PARITY"<<endl;
        trueMSGwParity = receivedMessage;
        corruption[0]='1'; //For corruption, change parity to 1 if it was 0 in the original message, and keep the check bit as it is to
        // lead the receiver to the original parity condition.
        errMSGwParity = corruption;
    }
    else{
        cout<<"ODD PARITY"<<endl;
        trueMSGwParity = receivedMessage;
        corruption[0]='0';//For corruption, change parity to 0 if it was 1 in the original message, and keep the check bit as it is to
        // lead the receiver to the original parity condition.
        errMSGwParity = corruption;
    }

//Randomly return true or wrong value (if even parity: true msg will be returned).
    if(trueMSGwParity.compare(0, 1, "0") == 0){
        return trueMSGwParity;
    }
    else {//(if odd parity: corrupted msg will be returned).
        return errMSGwParity;
    }

}

//Function Prototypes
void ServiceClient(client_type &new_client, vector<client_type> &client_array, thread &thread);
int main();

//step13 from main):
// the service thread:
//The ServiceClient function is a function that processes incoming messages from clients and takes appropriate actions based on the content of the message.
//The main purpose of this function is to handle communication between clients by implementing various commands. The following commands are implemented in this function:
//CONN: Notify a new client, sends a list of current clients, and notifies other clients a new person has joined the group.
//MESG: This command is used by a client to send a message to another client. The message is in the format MESG|<receiver name>->|parity|<message>|check bit|.
// When this command is received, we have some corruption scenario: if the message's parity was even, the server will send it without any corruption to the receiver.
// If the parity was odd, the server will corrupt it using corruptMSGrandom function which returns a randomly generated string with changed parity.
//Error checking scenario: The receiver will check the parity and the check bit, if they're equal, the message is true.
//if the parity and check bit aren't equal, then the message was corrupted.
// The receiver will send a "MERR-><receiver name>" command to the server, and the server will resend the message without any corruption to the receiver.
//GONE: removes an active client account and notifies all other members the client has left and kill the thread.

void ServiceClient(client_type &new_client, vector<client_type> &client_array, thread &thread)
{
    string msg = ""; //Variable for holding the msg to es sent.
    char tempmsg[DEFAULT_BUFLEN] = ""; //holds the received message from client
    string clientsInfo[DEFAULT_BUFLEN];// holds the clients' list that'll be sent to the client if CONN command received
    int iResult;//Holds the result of recv or send function
    string receiverName;//Holds the receiver name mentioned in MESG command.
    string corruptMSG;// Holds the returned value from corruptMSGrandom function.


    //1) an infinite loop that waits for a message from the client. If a message is received, the code checks which command it contains
    while (1)
    {
        string notify = "";//Holds the notification sent from server in case of any, like notifying other clients of the new client joined.

        memset(tempmsg, 0, DEFAULT_BUFLEN);//cleaning tempmsg (which holds the received message every time the loop starts)

        if (new_client.socket != 0) {//2) while the client that joined is connected:
            iResult = recv(new_client.socket, tempmsg, DEFAULT_BUFLEN, 0);//3) receive a message
            if (iResult != SOCKET_ERROR) {//4) if the recv function didn't return an error:
                //5) Check which command:
                if (string(tempmsg).compare(0, 4, "CONN") == 0) {//I) if it is CONN command:
                    cout << "CONN COMMAND RECEIVED: " << tempmsg << endl;
                    //1) Get name of client:
                    size_t pos = string(tempmsg).find_last_of("|");//2) find name of the client in the CONN comes after "|" ex: CONN|sam
                    if (pos != string::npos){//find_last_of function searches the string for the last occurrence of any character in a given characters(|).
                        // If it finds one, it returns the index of that character. If it does not find one, it returns string::npos
                        new_client.name[new_client.id] = string(tempmsg).substr(pos + 1);//to keep track of last active client in case of disconnection.
                        client_array[k].name[k] = string(tempmsg).substr(pos + 1);//save name into client array which holds the connected clients
                                }
                    memset(tempmsg, 0, DEFAULT_BUFLEN);
                    Sleep(100);
                    //3) Get ip from client **NOT NECESSARY ONLY IF WE HAD A PROBLEM GETTING THE IP FROM THE SERVER, WE WILL RECEIVE IT FROM CLIENT
                    //iResult = recv(new_client.socket, tempmsg, DEFAULT_BUFLEN, 0);
                    //if (iResult != SOCKET_ERROR) {
                        //string s2 = string(tempmsg);
//                        if (s2.compare(0, 3, "127") == 0)//UNCOMMENT IF IP ADDRESS IS NOT FOUND BY SERVER
//                            client_array[k].IP[k] = s2;
                        for (int i = 0; i < MAX_CLIENTS; i++) {//4) looping through the client_array to construct
                            // a string called client_Inf that contains information about each client in the array
                            // (e.g., their name, socket, and IP address) and storing it in the clientsInfo array.
                            //j variable is used for formatting the line number of the list, (starting from 1)
                            int j = i + 1;
                            client_Inf =
                                    to_string(j) + ") client: " + "name: " + client_array[i].name[i] + ", Socket: " +
                                    to_string(client_array[i].socket) + ", Ip: " +
                                    client_array[i].IP[i];
                            clientsInfo[i] = client_Inf.c_str();
                            //5) Notify others about the new client but not the same client who request CONN command (new_client.id != i)
                            if (client_array[i].socket != INVALID_SOCKET) {
                                if (new_client.id != i) {
                                    notify = "\n****Client " + client_array[new_client.id].name[new_client.id] +
                                             " Joined the chat****\n";
                                    iResult = send(client_array[i].socket, notify.c_str(), strlen(notify.c_str()), 0);
                                    cout << "Notified..." << endl;

                                }
                            }
                        }
                        //6) Send list to the client who request CONN
                        //The list is sent to one client at a time by looping through the client_array and sending
                        // each element of clientsInfo to the client who requested it using the send() function.
                        // The clientsInfo array holds the information about each client in the following format:
                        // "line number) client: name: client_name, Socket: client_socket, Ip: client_ip"
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (client_array[i].socket != INVALID_SOCKET) {
                                cout << clientsInfo[i].c_str() << endl;
                                iResult = send(new_client.socket, clientsInfo[i].c_str(),
                                               strlen(clientsInfo[i].c_str()),
                                               0);
                            }
                        }
                    //}
                }
                //II) if the received command contains "MESG" (indicating that it is a message intended for another client):
                else if (string(tempmsg).compare(0, 4, "MESG") == 0) {
                    cout << "MESG COMMAND RECEIVED: " << tempmsg << endl;
                    // Find the position of the '|' symbol
                    size_t pos1 = string(tempmsg).find("|");

                    // Find the position of the '->' symbol
                    size_t pos2 = string(tempmsg).find("->");

                    //1) Extract the name of the receiver
                    if (pos1 != string::npos && pos2 != string::npos) {
                        receiverName = string(tempmsg).substr(pos1 + 1, pos2 - pos1 - 1);
                    }
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        //2) searches through the list of clients to find a match (if the receiver is present or not).
                        if (client_array[i].name[i].find(receiverName) != string::npos) {
                            // receiverName was found in client_array[i].name[i]
                            if (client_array[i].socket != INVALID_SOCKET) {
                                if (new_client.id != i) {// 3) make sure that sending is not going to same client
                                    //4) Extract the message content
                                    if (size_t pos = string(tempmsg).find_last_of(">")) {
                                        if (pos != string::npos)
                                            receivedMessage = string(tempmsg).substr(pos + 1);
                                    //5) Extract senderName also
                                        senderName = client_array[new_client.id].name[new_client.id];//Get the senderName to use it in MERR condition later.
                                    //6) Corrupt or do not corrupt the message according to corruptMSGrandom's returned value.
                                        corruptMSG = corruptMSGrandom(
                                                receivedMessage);
                                        cout << corruptMSG
                                             << endl;//print the message to see if it was corrupted or not for controlling purpose only
                                    //7) Format the message for receiver:
                                        notify = "! Message from [" + client_array[new_client.id].name[new_client.id] +
                                                 "] to " +
                                                 receiverName + ": " + corruptMSG;
                                    //8) Send the message to the receiver.
                                        iResult = send(client_array[i].socket, notify.c_str(), strlen(notify.c_str()),
                                                       0);
                                        if (iResult !=
                                            SOCKET_ERROR) {// 9) If the message was successfully delivered, the loop breaks;else, print error
                                            cout << "MSG Delivered..." << endl;
                                            i = MAX_CLIENTS;
                                        } else {
                                            cout << "Message not delivered..." << endl;
                                        }

                                    }

                                }
                            }

                        }
                    }
                }
                //III) This code processes a request from a receiver client to resend a previously sent message that may have been corrupted.
                // If the command "MERR" is received:
                else if (string(tempmsg).compare(0, 4, "MERR") == 0) {
                    cout << "MERR COMMAND RECEIVED: " << tempmsg << endl;
                    //1) Get the name of the receiver again:MESG|name->hi
                    if (size_t pos = string(tempmsg).find_last_of("|")) {
                        if (pos != string::npos)
                            receiverName = string(tempmsg).substr(pos + 1);//added 3
                    }
                    //2) loop through the array of clients and checks if the client with the extracted name exists and is connected.
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (client_array[i].name[i].find(receiverName) != string::npos) {
                            // receiverName was found in client_array[i].name[i]
                            if (client_array[i].socket != INVALID_SOCKET) {
                                //3) format the message for the receiver
                                notify =
                                        "***RESEND CORRECTED MESSAGE**** Message from [" +
                                        senderName + "] to " +
                                        receiverName + ": " + receivedMessage;
                                //4) sends the previously received message to the client.
                                iResult = send(new_client.socket, notify.c_str(),
                                               strlen(notify.c_str()),
                                               0);
                                if (iResult !=
                                    SOCKET_ERROR) {//5) If the message was successfully delivered, the loop breaks;else, print error
                                    cout << "MSG Delivered again..." << endl;
                                    i = MAX_CLIENTS;
                                } else {
                                    cout << "Message not delivered..." << endl;
                                }

                            }
                        }
                    }
                }
                //IV) This code block handles a "GONE" command received from a client. When this command is received,
                // it means that the client with the corresponding socket wants to disconnect.
                else if (string(tempmsg).compare(0, 4, "GONE") == 0) {
                    //1) Format the notification for other clients:
                    msg = "**Client #" + to_string(new_client.id) + " name: " + new_client.name[new_client.id] +
                          " Disconnected";
                    cout << msg << endl;
                    //2) Send closing order to the sender client:
                    notify = "Disconnect";
                    iResult = send(new_client.socket, notify.c_str(),
                                   strlen(notify.c_str()),
                                   0);
                    //3) Close the socket for the disconnected client and set it to the invalid socket value (INVALID_SOCKET).
                    closesocket(new_client.socket);
                    closesocket(client_array[new_client.id].socket);
                    client_array[new_client.id].socket = INVALID_SOCKET;
                    Sleep(20);

                    //4) Delete the client's information from the clientsInfo array:
                    client_array[new_client.id].name[new_client.id] = "";
                    client_array[new_client.id].IP[new_client.id] = "";
                    clientsInfo[new_client.id] = "";

                    //5) Broadcast the disconnection message to all other !INVALID_SOCKET clients
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (client_array[i].socket != INVALID_SOCKET)
                            iResult = send(client_array[i].socket, msg.c_str(), strlen(msg.c_str()), 0);
                    }
                    //6) Update the client_info string by deleting the line corresponding to the disconnected client
                    size_t line = (new_client.id + 1) * 3; // 3 is the number of lines per client in the client_inf string
                    if (line < client_Inf.size()) {
                        // Delete the line from the client_inf string
                        client_Inf.erase(line, 3);
                    }
                    //7) Reset the socket for the new client
                    new_client.socket = INVALID_SOCKET;

                    //8) Update the last_active_client variable to keep track of clients;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (client_array[i].socket != INVALID_SOCKET) {
                            last_active_client = i;
                        }
                    }
                    break;
                    //then the loop is broken, ending the connection with the disconnected client.

                }
            }
            else{//If client connection lost, repeat the GONE command steps:

                msg = "Client #" + to_string(new_client.id) + " name: " + new_client.name[new_client.id] + " Disconnected";
                cout << msg << endl;
                notify = "Disconnect";
                iResult = send(new_client.socket, notify.c_str(),
                               strlen(notify.c_str()),
                               0);

                closesocket(new_client.socket);
                closesocket(client_array[new_client.id].socket);
                client_array[new_client.id].socket = INVALID_SOCKET;
                Sleep(20);
                // Delete the client's information from the clientsInfo array
                client_array[new_client.id].name[new_client.id] = "";
                client_array[new_client.id].IP[new_client.id] = "";
                clientsInfo[new_client.id] = "";

                //Broadcast the disconnection message to the other clients
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_array[i].socket != INVALID_SOCKET)
                        iResult = send(client_array[i].socket, msg.c_str(), strlen(msg.c_str()), 0);
                }
                // Update the client_info string by deleting the line corresponding to the disconnected client
                size_t line = (new_client.id + 1) * 3; // 3 is the number of lines per client in the client_inf string
                if (line < client_Inf.size()) {
                    // Delete the line from the client_inf string
                    client_Inf.erase(line, 3);
                }

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_array[i].socket != INVALID_SOCKET) {
                        last_active_client = i;
                    }
                }
                break;
            }
        }
        k++;
    }//end while
}

int main()
{
    WSADATA wsaData; // wsaData is a variable of type WSADATA that is used to store information about,
    // the Windows Sockets implementation returned by WSAStartup function.

    struct addrinfo hints, *res;//hints is a variable of type addrinfo that is used to specify hints,
    // about the address type: ex) AF_INET for IPv4 , socket type: SOCK_STREAM, protocol : IPPROTO_TCP,
    //res is a pointer to an addrinfo structure that is used to store the result of the getaddrinfo function, which is used to get a,
    // list of address structures that match the specified hints.

    struct addrinfo *server = NULL;// a pointer to an addrinfo structure.used to represent an address,
    // in the Internet address family. like: ai_family,ai_socketype etc.

    struct in_addr addr;//in_addr is a structure used to represent an IPv4 address in the Internet address family.
    SOCKET server_socket = INVALID_SOCKET;//server_socket is a variable of type SOCKET that is used to store a socket,
    //INVALID_SOCKET value is often used as a placeholder to indicate that the socket has not yet been created or has been closed.

    string msg = "";//String variable to hold messages that will be sent to the clients

    vector<client_type> client(MAX_CLIENTS);//creates a vector object called client that can store objects of type client_type
    //The vector class is a useful container type because it provides dynamic resizing and easy access.

    int num_clients = 0;//counter for clients connected.

    int temp_id = -1;

    thread mythread[MAX_CLIENTS];//declares an array of thread objects called mythread with a size of MAX_CLIENTS.

    char addrstr[100];//array to hold address info.

    void *ptr;

    //1) Initialize Winsock
    cout << "Intializing Winsock..." << endl;
    WSAStartup(MAKEWORD(2, 2), &wsaData);// returns information about the Windows Sockets implementation.

    //2) Setup hints;
    //These values for the fields of the hints structure can be used to specify hints about the
    // type of address the caller is interested in when calling functions such as getaddrinfo.

    ZeroMemory(&hints, sizeof(hints));//sets all of the bytes in the hints structure to zero,
    // This effectively initializes all of the fields of the hints structure to their default values.

    hints.ai_family = AF_INET;//pecifies that the address is an IPv4 address.
    hints.ai_socktype = SOCK_STREAM;//A stream socket is a type of socket that provides a reliable, tcp connection between two sockets.
    hints.ai_protocol = IPPROTO_TCP;//specifies that the TCP protocol is to be used with the socket.reliable, stream-oriented protocol that is used to transmit data over the Internet.
    hints.ai_flags = AI_PASSIVE;//specifies that the address is
    // intended for use as a bind address for a server.


    //3) Setup Server
    cout << "Setting up server..." << endl;
    //getaddrinfo: returns a list of addrinfo structures that match the specified hints in the server variable.
    getaddrinfo(static_cast<LPCTSTR>(IP_ADDRESS), DEFAULT_PORT, &hints, &server);
    //nodename: a pointer to numerical network address of the target system.LPCTSTR type, which is a pointer to a constant TCHAR string.
    //servname: a pointer to specify port number.
    //hints:a pointer to an addrinfo structure that specifies hints about the type of address.
    //res: a pointer to an addrinfo structure that is used to store the result of the getaddrinfo function, which is used to get a
    // list of address structures that match the specified hints.
    res = server;//sets res to point to the same addrinfo structure as server.

    //4) Create a listening socket for connecting to server
    cout << "Creating server socket..." << endl;
    server_socket = socket(server->ai_family, server->ai_socktype, server->ai_protocol);// calling the socket function with the provided arguments.

    //5) Setup socket options
    //setsockopt is a function that is used to set the options associated with a socket.
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &OPTION_VALUE, sizeof(int)); //Make it possible to re-bind to a port that was used within the last 2 minutes
    //The SO_REUSEADDR option allows multiple sockets to be bound to the same address.

    setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &OPTION_VALUE, sizeof(int));
    //he TCP_NODELAY option disables the Nagle algorithm for the socket,
    // which can improve the performance of small data transfers by allowing the data to be sent immediately, rather than waiting for a full packet.

    //6) Assign an address to the server socket.
    cout << "Binding socket..." << endl;
    bind(server_socket, server->ai_addr, (int)server->ai_addrlen);//The bind function binds the socket
    // identified by server_socket to the address and port specified in the server->ai_addr structure.

    //7) Listen for incoming connections.
    cout << "Listening..." << endl;
    listen(server_socket, SOMAXCONN);
    //The listen function marks the socket identified by server_socket as a listening socket and listen for
    // incoming connection requests on the socket, and allows the socket to queue incoming connection requests while
    // they are waiting to be accepted.
    // It specifies the maximum length of the queue of pending connections as SOMAXCONN.

    //8) Initialize the client list
    // for loop iterates over the elements of the client array and initializes each element to a client_type object with default values.
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client[i] = { -1, INVALID_SOCKET};
    }



    //client_addr is a sockaddr_in structure that will receive the address of the connecting entity.
    // addrlen is an integer that specifies the length of the client_addr structure.
    struct sockaddr_in client_addr;
    int addrlen = sizeof(client_addr);
    struct in_addr in;

    //9) an infinite while loop that listens for incoming connection requests on the socket and accepts them when they arrive.
    while (1)
    {

        SOCKET incoming = INVALID_SOCKET;
        //10) accept function returns a new socket descriptor that is used to communicate with the client.
        // If the accept function fails, it returns INVALID_SOCKET.
        incoming = accept(server_socket,  (struct sockaddr *)&client_addr, &addrlen);

        if (incoming == INVALID_SOCKET) continue;

        //Reset the number of clients
        num_clients = -1;

        // "in" is a struct in_addr that is initialized with the s_addr field of the client_addr.sin_addr field.
        // addr is a pointer to a char array that contains the string representation of the address.
        in.s_addr = client_addr.sin_addr.s_addr;

        //inet_ntoa function takes a struct in_addr as an argument and returns a pointer to a char array
        // that contains the string representation of the address.
        char *addr = inet_ntoa(in);

        //Converting char array to string
        string str_addr(addr);

        //11) Storing incoming cients:

        //temp_id variable is used to store a temporary ID for the next client that connects to the server.
        temp_id = -1;
        //The temp_id variable is initialized to -1 at the beginning of the while loop.
        // The while loop then calls the accept function to accept an incoming connection request on the socket.

        //After the connection is accepted, the for loop iterates over the elements of the client array and looks for an
        // element that has an invalid socket (INVALID_SOCKET) and a temporary ID of -1. When it finds such an element,
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client[i].socket == INVALID_SOCKET && temp_id == -1)
            {   // it sets the socket of the element to the incoming socket and the ID of the element to the index of the element in the array.
                client[i].socket = incoming;
                client[i].id = i;
                client[i].IP[i] = str_addr;//COMMENT IF IP IS NOT FIGURED BY SERVER
                temp_id = i;

            }
            //incrementing number of connected clients
            if (client[i].socket != INVALID_SOCKET) {
                num_clients++;
            }
        }

        //The if statement (temp_id != -1) checks if the temporary ID of the client is not -1. (was set to i in previous operation)
        // If the temporary ID is not -1, it means that an element in the client array was found and the client was added to the client array.
        if (temp_id != -1)
        {   // The temporary ID is then set to the index of the element in the array
            cout << "Client #" << client[temp_id].id << " Accepted" << endl;

            //12) sending the ID of the client to the client
            msg = to_string(client[temp_id].id);
            send(client[temp_id].socket, msg.c_str(), strlen(msg.c_str()), 0);

            //13) Create a thread for that client (SEE ServiceClient ABOVE)
            mythread[temp_id] = thread(ServiceClient, ref(client[temp_id]), ref(client), ref(mythread[temp_id]));

            //14) Update last_active_client
            //To keep track of the last active client, we define a variable called last_active_client and initialize it to -1.
            // Then, in the loop that adds a new client, you can check if last_active_client is less than MAX_CLIENTS - 1,
            // and if it is, you can add the new client at the index last_active_client + 1
            if (temp_id > last_active_client) {
                last_active_client = temp_id;
            }
        }
        else
        {//If all elements of the client array are already in use, the temp_id variable remains -1 and the client is not added to the client array.
            msg = "Server is full";
            send(incoming, msg.c_str(), strlen(msg.c_str()), 0);
            cout << msg << endl;
        }
        mythread[temp_id].detach(); //detach = disconnect. calling the detach function on it will detach the thread from the calling thread,

    } //end while


    //Close listening socket
    closesocket(server_socket);

    //Close client socket
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        //my_thread[i].detach();
        closesocket(client[i].socket);
    }

    //Clean up Winsock
    WSACleanup();
    cout << "Program has ended successfully" << endl;

    system("pause");
    return 0;
}










//Github: @olaDEN