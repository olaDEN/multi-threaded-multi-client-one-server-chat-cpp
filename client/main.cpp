#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <functional> //for std::function
#include <time.h>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <experimental/filesystem>
#include <mutex>

using namespace std;
namespace fs = std::experimental::filesystem;
std::mutex file_mutex; // Declare a mutex object for locking file while writing.

#define DEFAULT_BUFLEN 32768
#define IP_ADDRESS "192.168.43.125"
#define DEFAULT_PORT "5555"

string connectedClients[DEFAULT_BUFLEN];
unsigned first; 
unsigned last; 
time_t my_time = time(NULL);
char *ctime_no_newline = strtok(ctime(&my_time), "\n");
stringstream file_name_ss; //file_name_ss is an object of type stringstream that can be used to read and write data from a string.
string file_name;//string to hold the file name after formatting it.
ofstream file;
int a = 0;


//Adding parity and check bits for the sent_message before sending it to server:
string addParityToMSG(string receivedMessage, string receiver){
    // Initialize variables
    string request;
    size_t length = static_cast<size_t>(receivedMessage.length());
    string binaryMSG;
    string MSGwParity;
    int parity = 0;

    // Convert the input string to a binary string
    for (size_t i = 0; i < length; ++i) {
        char c = receivedMessage[i];
        for (int j = 7; j >= 0; --j) {
            binaryMSG += ((c >> j) & 1) + '0';
        }
    }

    // Calculate parity
    for (size_t i = 0; i < binaryMSG.length(); ++i) {
        parity ^= (binaryMSG[i] - '0');
    }

    // Format the message with parity and check bits; (in the true message, the parity always equal to check bit,
    // if different, then the message was corrupted).
    string check = to_string(parity);
    MSGwParity = receiver + "->" + check + "|" + receivedMessage + "|" + check;

    return MSGwParity;

}

//A function to get the ip of the client if server couldn't figure it by itself.
string getIP() {
    char ac[80];
    //calling gethostname, to get the host name of the machine and stores it in the character array ac.
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
        cerr << "Error " << WSAGetLastError() <<
             " when getting local host name." << endl;
        return "";
    }

    struct hostent *phe = gethostbyname(ac);//gethostbyname returns a struct hostent pointer, which contains information about the host, such as its IP address.
    if (phe == 0) {
        cerr << "Bad host lookup." << endl;
        return "";
    }

    struct in_addr addr;
    memcpy(&addr, phe->h_addr_list[0], sizeof(struct in_addr));//retrieve the first IP address in the
    // list stored in the h_addr_list field of the struct hostent, and stores it in a struct in_addr called addr.
    char* data = inet_ntoa(addr);//inet_ntoa returns a string representation of the IP address.
    string myString(data, strlen(data));//convert it to string type and return.
    return myString;
}

//A struct that represents a client in the chat. It has three fields:
//socket: a socket descriptor that is used to communicate with the client.
//id: an integer that represents the client's ID, received by the server.
//name: a string that represents the client's name.
//received_message: a char array that stores the message received from the server.
//file_name: a string that represents the file name.
//file: an output filestream that is used to write data to a file.
struct client_type
{
    SOCKET socket;
    int id;
    string name;
    char received_message[DEFAULT_BUFLEN];
    string file_name;
    ofstream file;
};

int ReceiveChat(client_type &new_client);
void CONN_Command(client_type &client, basic_string<char> ipADDR);
int main();


int ReceiveChat(client_type &new_client) {
    string receivedMSG = "";//Variable to hold message received from server.
    string MSGwithParity = "";//Variable to hold message returned from addParityToMSG function.
    int n;
    std::string data;

    while (1) { //An infinite loop to receive from server while connection is valid.
        memset(new_client.received_message, 0, DEFAULT_BUFLEN);

        if (new_client.socket != 0) {
            //First thing sent from server after the id of the client received in main, is the response to CONN_Command sent
            // automatically from client using CONN_command function below.
            int iResult = recv(new_client.socket, new_client.received_message, DEFAULT_BUFLEN, 0);
            if (iResult != SOCKET_ERROR) {
                cout <<"[" << ctime_no_newline<<"]"<<":\n"<< new_client.received_message << endl;//Print the client lists;
                file_mutex.lock();
                new_client.file << "[" << ctime_no_newline<<"]"<<":\n"<< new_client.received_message << std::endl;
                //The flush() function is used to flush the output buffer of a stream. 
                //It is used to ensure that all the data in the buffer is written to the destination (e.g., a file, the console).
                new_client.file.flush();
                file_mutex.unlock();
                Sleep(5);
                if (string(new_client.received_message).compare(3, 6, "client") == 0) {
                    //If client list arrived, add it to connectedClients array;
                    connectedClients[a] = string(new_client.received_message);
                }
                //The format for a message that'll be sent from other client through the server will start with "!", so,
                // whenever the client receives a message starts with "!", it should perform parity check oepration.
                else if (string(new_client.received_message).compare(0, 1, "!") == 0) {
                    receivedMSG = string(new_client.received_message);
                    Sleep(500);
                    size_t pos = receivedMSG.find(": ");
                    if (pos != string::npos)
                        MSGwithParity = receivedMSG.substr(pos + 2); // Extract the important message content that will start
                        // after ":" in the string received from server.
                        cout <<"[" << ctime_no_newline<<"]"<<": "<<MSGwithParity << endl;
                        file_mutex.lock();
                        new_client.file <<"[" << ctime_no_newline<<"]"<<": "<<MSGwithParity << std::endl;
                        new_client.file.flush();
                        file_mutex.unlock();
                        Sleep(5);

                    n = MSGwithParity.length();
                    if (MSGwithParity[0]!=MSGwithParity[n-1]) {//if parity bit of the message and the check bits are not equal, SEND MERR to server.
                        cout <<"[" << ctime_no_newline<<"]"<<": "<<"ERROR DETECTED, INCORRECT MESSAGE ARRIVED\nSending MERR command to server..." << endl;
                        file_mutex.lock();
                        new_client.file <<"[" << ctime_no_newline<<"]"<<": "<<"ERROR DETECTED, INCORRECT MESSAGE ARRIVED\nSending MERR command to server..." << std::endl;
                        new_client.file.flush();
                        file_mutex.unlock();
                        Sleep(5);
                        string MERR = "MERR|"+new_client.name;
                        send(new_client.socket, MERR.c_str(), strlen(MERR.c_str()), 0);
                    }
                    else{
                        cout <<"[" << ctime_no_newline<<"]"<<": "<<"NO ERROR DETECTED, MESSAGE ARRIVED IS CORRECT\n" << endl;
                        file_mutex.lock();
                        new_client.file <<"[" << ctime_no_newline<<"]"<<": "<<"NO ERROR DETECTED, MESSAGE ARRIVED IS CORRECT\n" << std::endl;
                        new_client.file.flush();
                        file_mutex.unlock();
                        Sleep(5);
                    }
                }//If the client received disconnet order from server,it'll disconnect.
                else if (string(new_client.received_message).compare(0, 10, "Disconnect") == 0) {
                    cout <<"[" << ctime_no_newline<<"]: Client is Disconnected" << endl;
                    new_client.file <<"[" << ctime_no_newline<<"]: Client is Disconnected" << std::endl;
                    new_client.file.flush();
                    file_mutex.unlock();
                    Sleep(5);
                    WSACleanup();
                    new_client.file.close();//Close the log file.
                    system("pause");
                }

            }
            else {
                cout <<"[" << ctime_no_newline<<"]"<<": "<<"recv() failed: " << WSAGetLastError() << endl;
                file_mutex.lock();
                new_client.file <<"[" << ctime_no_newline<<"]"<<": "<<"recv() failed: " << WSAGetLastError() << std::endl;
                //The flush() function is used to flush the output buffer of a stream. 
                //It is used to ensure that all the data in the buffer is written to the destination (e.g., a file, the console).
                new_client.file.flush();
                file_mutex.unlock();
                Sleep(5);
                break;
            }
        }
        a++;
    }
    if (WSAGetLastError() == WSAECONNRESET)
        cout <<"[" << ctime_no_newline<<"]"<<": "<<"The server has disconnected" << endl;
        file_mutex.lock();
        new_client.file <<"[" << ctime_no_newline<<"]"<<": "<<"The server has disconnected" << std::endl;
        //The flush() function is used to flush the output buffer of a stream. 
        //It is used to ensure that all the data in the buffer is written to the destination (e.g., a file, the console).
        new_client.file.flush();
        file_mutex.unlock();
        Sleep(5);
        return 0;
}

void CONN_Command(client_type &client, basic_string<char> ipADDR){
    int iResult;
    std::string data;
    string CONN = "CONN|" + client.name;
    iResult = send(client.socket, CONN.c_str(), strlen(CONN.c_str()), 0);
    Sleep(100);
    //UNCOMMENT TO SEND IP TO SERVER IF SERVER COULDN'T RETRIVE IT.
    //send(client.socket,ipADDR.c_str(), strlen(ipADDR.c_str()), 0);
    //Sleep(5);
    //cout <<"[" << ctime_no_newline<<"]"<<": "<<"IP sent" << endl;
    //file_mutex.lock();
    //new_client.file <<"[" << ctime_no_newline<<"]"<<": "<<"IP sent" << std::endl;
    //new_client.file.flush();
    //Sleep(5);

}

int main() {
    WSAData wsa_data;// wsaData is a variable of type WSADATA that is used to store information about,
    // the Windows Sockets implementation returned by WSAStartup function.

    struct addrinfo *result = NULL, *ptr = NULL, hints;//*result and *ptr are pointers to struct addrinfo structure that is
    // used to hold the address information returned by getaddrinfo function.
    // hints is a variable of type addrinfo that is used to specify hints
    // about the address type: ex) AF_INET for IPv4 , socket type: SOCK_STREAM, protocol : IPPROTO_TCP.

    string sent_message = "";//used to store the message that the user inputs from the command line using the getline function.
    // This message will be sent to the server using the send function later on in the code.

    client_type client = {INVALID_SOCKET, -1, ""};//Initializing default values to client_type struct
    int iResult = 0;//iResult is used to store the result of the recv and send function.

    string message;// message variable is being assigned the value of client.received_message from server later on.
    string MSGwithParity;// MSGwithParity is a string that is created by calling the addParityToMSG function to add a
    // parity and check bits to the sent_message string when sending a message to other clients.
    string receiver;// Holds the receiver's name specified after "MESG|" in the string of sent_message variable.

    // 1) Get the current time and date for file name
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    tm local_tm = *localtime(&tt);

    // 2) Create a folder named "logs" if it doesn't already exist
    if (!fs::exists("../logs")) {
        fs::create_directory("../logs");
    }
    // 3) Get client name:
    cout << "[" << ctime_no_newline << "]" << ": " << "Enter your name: " << endl;
    getline(cin, client.name);//get the input from user and save it in client.name variable.

    //4) Create the file name based on the current date and time and the client's name
    file_name_ss << "../logs/" << put_time(&local_tm, "%Y%m%d-%H%M") << "-"
                 << client.name << ".txt";//format the file name, so it doesn't contain any invalid chars in file name like ":"
    file_name = file_name_ss.str();//convert stringstream into string to save file name.

    //5) Create and open the file
    client.file.open(file_name,std::ios::app );
    if (client.file.is_open()) {//if the file was open: continue, if not, print error and do not continue the program.

        file_mutex.lock();// using the lock() and unlock() functions of the mutex object,  before every file write
        // operation, and release the lock after the write operation.
        // This ensures that only one thread can write to the file at a time, preventing conflicts and race conditions
        client.file << "Log file for client " << client.name << "\n";
        client.file << "Created on " << put_time(&local_tm, "%A %B %d %Y %I:%M:%S %p") << "\n";
        client.file_name = file_name;

        //6) Write the previous console's output into file
        client.file << "[" << ctime_no_newline << "]" << ": " << "Enter your name: " << std::endl;
        client.file.flush();//The flush() function is used to is used to ensure that all the data in the buffer is written to
        // the destination (e.g., a file, the console).
        Sleep(5);
        client.file << "[" << ctime_no_newline << "]" << ": " << client.name << std::endl;
        client.file.flush();
        Sleep(5);

        cout << "[" << ctime_no_newline << "]" << ": " << "Starting Client..." << endl;
        client.file << "[" << ctime_no_newline << "]" << ": " << "Starting Client..." << endl;
        client.file.flush();
        Sleep(5);

        //7) Initialize Winsock and getting client's ip:

        WSAData wsaData;
        if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
            return 255;
        }
        string ipADDR = getIP(); // Get client ip if server couldn't find it automatically due to some errors.
        iResult = WSAStartup(MAKEWORD(2, 2), &wsa_data);// returns information about the Windows Sockets implementation.
        //MAKEWORD(2, 2) creates a 16-bit word that represents the version of the Winsock library being requested.
        if (iResult != 0) {//checking the result of wsastartup:
            cout << "[" << ctime_no_newline << "]" << ": " << "WSAStartup() failed with error: " << iResult << endl;
            client.file << "[" << ctime_no_newline << "]" << ": " << "WSAStartup() failed with error: " << iResult
                        << std::endl;
            client.file.flush();
            Sleep(5);
            return 1;
        }

        //8) Setup hints;
        //These values for the fields of the hints structure can be used to specify hints about the
        //type of address the caller is interested in when calling functions such as getaddrinfo.
        ZeroMemory(&hints, sizeof(hints));// Sets all of the bytes in the hints structure to zero,
        // This effectively initializes all of the fields of the hints structure to their default values.
        hints.ai_family = AF_UNSPEC;// Specifies that the address is an IPv4 address.
        hints.ai_socktype = SOCK_STREAM;// A stream socket is a type of socket that provides a reliable, tcp connection between two sockets.
        hints.ai_protocol = IPPROTO_TCP;// Specifies that the TCP protocol is to be used with the socket.reliable, stream-oriented protocol that is
        // used to transmit data over the Internet.

        cout << "[" << ctime_no_newline << "]" << ": " << "Connecting..." << endl;
        client.file << "[" << ctime_no_newline << "]" << ": " << "Connecting..." << std::endl;
        client.file.flush();
        Sleep(5);

        //9) Resolve the server address and port
        //getting the address of the server, IP_ADDRESS is the IP address of the server and
        // DEFAULT_PORT is the port number on which the server is listening for connections.
        // The function getaddrinfo takes these and returns a linked list contains information about the server's address,
        // including its IP address and port number.
        iResult = getaddrinfo(static_cast<LPCTSTR>(IP_ADDRESS), DEFAULT_PORT, &hints, &result);
        if (iResult != 0) {
            cout << "[" << ctime_no_newline << "]" << ": " << "getaddrinfo() failed with error: " << iResult << endl;
        client.file << "[" << ctime_no_newline << "]" << ": " << "getaddrinfo() failed with error: " << iResult
                        << std::endl;
            client.file.flush();
            Sleep(5);
            WSACleanup();// Used to decrement the count of the number of threads that have called WSAStartup.
            // When the count reaches zero, WSACleanup closes any sockets still open and frees resources.
            system("pause");
            return 1;// The return 1 statement at the end of the code block terminates the main function and returns a
            // value of 1 to the operating system. This can be used to indicate that the program encountered an error and terminated.
        }

        //10) Try to connect to an address until one succeeds:
        //The for loop iterates through a linked list of addrinfo structures pointed to by result.
        // The loop will continue until ptr becomes NULL, which indicates the end of the linked list.
        // The ptr variable is updated to point to the next element in the linked list by setting it equal to
        // ptr->ai_next at the end of each iteration.
        for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        //11) Create a SOCKET for connecting to the server
            client.socket = socket(ptr->ai_family, ptr->ai_socktype,
                                   ptr->ai_protocol);
            if (client.socket == INVALID_SOCKET) {
                cout << "[" << ctime_no_newline << "]" << ": " << "socket() failed with error: " << WSAGetLastError()
                     << endl;
                client.file << "[" << ctime_no_newline << "]" << ": " << "socket() failed with error: " << WSAGetLastError()
                            << std::endl;
                client.file.flush();
                Sleep(5);
                WSACleanup();
                system("pause");
                return 1;
            }

            //12) Connect to the server.
            // connect function to establish a connection to connect to the server.
            iResult = connect(client.socket, ptr->ai_addr, (int) ptr->ai_addrlen);
            if (iResult == SOCKET_ERROR) {
                closesocket(client.socket);
                client.socket = INVALID_SOCKET;
                continue;
            }
            break;
        }

        freeaddrinfo(result);// called after using the addrinfo structures to release the memory and prevent memory leaks.

        //13) Check the client.socket's condition:
        if (client.socket == INVALID_SOCKET) {
            cout << "[" << ctime_no_newline << "]" << ": " << "Unable to connect to server!" << endl;
            client.file << "[" << ctime_no_newline << "]" << ": " << "Unable to connect to server!" << std::endl;
            client.file.flush();
            Sleep(5);
            WSACleanup();
            system("pause");
            return 1;
        }

        cout << "[" << ctime_no_newline << "]" << ": " << "Successfully Connected" << endl;
        client.file << "[" << ctime_no_newline << "]" << ": " << "Successfully Connected" << std::endl;
        client.file.flush();
        file_mutex.unlock();
        Sleep(5);



        //14) After connecting to the server, if the server isn't full, it'll return the client's id. Gettng the id specified from server:
        recv(client.socket, client.received_message, DEFAULT_BUFLEN, 0);
        message = client.received_message;
        if (message != "Server is full") {
            client.id = atoi(client.received_message);
            Sleep(5);
            thread my_thread(ReceiveChat, ref(client));//Open thread for this connection to handle receiving messages while sending commands from main
            CONN_Command(ref(client), ipADDR);//CONN_Command method that sends a CONN command to the Server as soon as the connection established.

        //15) start the chatting with the server:
            Sleep(100);
            cout << "[" << ctime_no_newline << "]" << ": " << "\n--To send a message, use this format: MESG|[receiver name]->message\n--To print a list of connected clients: CONN\n--To disconnect: GONE\n" << endl;
            file_mutex.lock();
            client.file << "[" << ctime_no_newline << "]" << ": " << "--To send a message, use this format: MESG|[receiver name]->message\n--To print a list of connected clients: CONN\n--To disconnect: GONE\n" << std::endl;
            client.file.flush();
            file_mutex.unlock();
            Sleep(5);

            // 16) A loop that waits for the user to input a message.(it could be CONN,MESG,GONE)
            while (1) {
                getline(cin, sent_message);//1) getting the message from user and save it in sent_message var.
                file_mutex.lock();
                client.file << "[" << ctime_no_newline << "]" << ": " << sent_message << std::endl;
                client.file.flush();
                file_mutex.unlock();
                Sleep(5);
                //2) if the message is one of the following three predefined commands: "CONN", "MESG", or "GONE".
                if (sent_message.compare(0, 4, "CONN") == 0 || sent_message.compare(0, 4, "MESG") == 0 || sent_message.compare(0, 4, "GONE") == 0) {
                    if (sent_message.compare(0, 4, "MESG") == 0) {
                        //I) If the input message starts with the string "MESG",
                        first = sent_message.find("MESG");
                        last = sent_message.find_last_of("-");
                        // Extract the receiver name between "MESG|" and "->" from the input string.
                        receiver = sent_message.substr(first, last - first);
                        if (size_t pos = sent_message.find_last_of(">")) {
                            if (pos != string::npos) {
                                sent_message = sent_message.substr(pos + 1);//Get the message content after ">" symbol.
                            }
                            MSGwithParity = addParityToMSG(sent_message, receiver);//The addParityToMSG function is
                            // use to add a parity and check bits to the message content to help detect errors.
                            // when the message is transmitted. The receiver variable will help to keep the intended recipient of the sent_message.
                            iResult = send(client.socket, MSGwithParity.c_str(), strlen(MSGwithParity.c_str()), 0);//Send to server.
                        }
                }else {// II) Not MESG but either CONN or GONE:  send directly to server.
                        iResult = send(client.socket, sent_message.c_str(), strlen(sent_message.c_str()), 0);
                    }
                } else {//III) Not one of the predefined command:
                    cout<< "[" << ctime_no_newline << "]" <<": "<< "Wrong input, please use the predefined commands CONN, MESG, GONE"<<endl;
                    file_mutex.lock();
                    client.file << "[" << ctime_no_newline << "]" << ": " << "Wrong input, please use the predefined commands CONN, MESG, GONE" << WSAGetLastError()
                                << std::endl;
                    client.file.flush();
                    file_mutex.unlock();
                    Sleep(5);
                }
                if (iResult <= 0) {
                    cout << "[" << ctime_no_newline << "]" << ": " << "send() failed: " << WSAGetLastError() << endl;
                    file_mutex.lock();
                    client.file << "[" << ctime_no_newline << "]" << ": " << "send() failed: " << WSAGetLastError()
                                << std::endl;
                    client.file.flush();
                    file_mutex.unlock();
                    Sleep(5);
                }

            }

            //17) Shutdown the connection, no more data will be sent
            my_thread.detach();
        } else{//If server is full, print the message coming from server.
            cout << "[" << ctime_no_newline << "]" << ": " << client.received_message << endl;
            file_mutex.lock();
            client.file << "[" << ctime_no_newline << "]" << ": " << client.received_message << std::endl;
            client.file.flush();
            file_mutex.unlock();
            Sleep(5);
        }
        cout <<"[" << ctime_no_newline<<"]"<<": "<<"Shutting down socket..." << endl;
        file_mutex.lock();
        client.file <<"[" << ctime_no_newline<<"]"<<": "<<"Shutting down socket..." << endl;
        client.file.flush();
        file_mutex.unlock();
        Sleep(5);

        iResult = shutdown(client.socket, SD_SEND);//The shutdown function is used to shutdown one or both of
        // the send and receive channels for a socket. the SD_SEND flag is passed as the second argument, which means that the send channel is to be shutdown.
        if (iResult == SOCKET_ERROR) {
            cout <<"[" << ctime_no_newline<<"]"<<": "<<"shutdown() failed with error: " << WSAGetLastError() << endl;
            file_mutex.lock();
            client.file <<"[" << ctime_no_newline<<"]"<<": "<<"shutdown() failed with error: " << WSAGetLastError() << std::endl;
            client.file.flush();
            file_mutex.unlock();
            Sleep(5);

            closesocket(client.socket);//Close the socket
            //Clean up Winsock
            WSACleanup();//used to decrement the count of the number of threads that have called WSAStartup.
            system("pause");
            return 1;
        }

    } else {//If the log file wasn't open at the beginning, nothing will happen and this message wil show up
        cerr << "Error creating log file for client " << client.name << "\n";
        return 1;
    }

    closesocket(client.socket);
    WSACleanup();
    client.file.close();//Close the log file.
    system("pause");
    return 0;
}










//Source: Github : @olaDEN
