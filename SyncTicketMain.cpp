//
// Created by Mert Dedekoy on 12.02.2021.
//

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>

using namespace std;

#define NUM_TELLERS 3

pthread_mutex_t mutexFile = PTHREAD_MUTEX_INITIALIZER;

typedef int buffer_item;
buffer_item buffer[NUM_TELLERS]; /* Teller buffer, value 1 means teller is available. 0 means teller is unavailable */

string inputPath;
string outputPath;
ofstream outputFile;

sem_t full; /* Semaphore for tellers */

pthread_t tid[NUM_TELLERS]; /* Teller threads' ids */

map<int, int> ticketMap; /* seat number - availability(0 = unavailable, 1 = available) map */
int clientCount; /* number of client */
int totalTicketNumber; /* total seat number */

void fillTicketMap(int size);

void *tellerHandler(void *param); /* Teller thread */

void *clientHandler(void *param); /* Client thread */

struct args { /* struct for sending client data to client thread */
    string clientName;
    int arrivalTime;
    int serviceTime;
    int seatNumber;
};

int main(int argc, char *argv[]) {
    /* get comment line arguments */
    inputPath = argv[1];
    outputPath = argv[2];

    if (sem_init(&full, 0, 3) == -1) { /* semaphore initialization */
        printf("%s\n", strerror(errno));
    }

    /* critical section, print welcome message, */
    pthread_mutex_lock(&mutexFile);
    outputFile.open(outputPath, ios::trunc);
    outputFile << "Welcome to the Sync-Ticket!" << endl;
    outputFile.close();
    pthread_mutex_unlock(&mutexFile);

    for (int i = 0; i < NUM_TELLERS; i++) {
        pthread_create(&tid[i], NULL, tellerHandler, NULL);
        usleep(2000);
    }

    for (int i = 0; i < NUM_TELLERS; i++) {
        pthread_join(tid[i], NULL);
    }

    /* read first and second line of the input file */
    ifstream infile(inputPath);
    string line;
    int lineCounter = 1;
    while (std::getline(infile, line)) {
        if (lineCounter == 1) { /* set available seats */
            if (line.substr(0, 1) == "O") { /* "O" is for ODA TIYATROSU */
                totalTicketNumber = 60;
            } else if (line.substr(0, 1) == "U") { /* "U" is for USKUDAR STUDYO SAHNE */
                totalTicketNumber = 80;
            } else if (line.substr(0, 1) == "K") { /* "K" is for KUCUK SAHNE */
                totalTicketNumber = 200;
            }
            fillTicketMap(totalTicketNumber);
        } else if (lineCounter == 2) { /* get client count */
            clientCount = stoi(line);
            break;
        }
        lineCounter++;
    }

    pthread_t cid[clientCount]; /* create client tread ids */

    /* read client information lines of the input file */
    ifstream in2file(inputPath);
    lineCounter = 1;
    int clientThreadCounter = 0;
    while (std::getline(in2file, line)) {
        if (lineCounter > 2 && lineCounter <= clientCount + 2) {
            vector <string> tokens;
            string token;
            istringstream tokenStream(line);
            while (std::getline(tokenStream, token, ',')) {  /* parse each line */
                tokens.push_back(token);
            }
            struct args *arguments = (struct args *) malloc(sizeof(struct args));
            arguments->clientName = tokens[0];
            arguments->arrivalTime = stoi(tokens[1]);
            arguments->serviceTime = stoi(tokens[2]);
            arguments->seatNumber = stoi(tokens[3]);

            /* start each client thread */
            pthread_create(&cid[clientThreadCounter], NULL, clientHandler, (void *) arguments);
            clientThreadCounter++;
        }
        lineCounter++;
    }

    /* join client threads */
    for (int i = 0; i < clientCount; ++i) {
        pthread_join(cid[i], NULL);
    }

    sem_destroy(&full);

    outputFile.open(outputPath, ios::app);
    outputFile << "All clients received service." << endl;
    outputFile.close();
    return 0;
}

void fillTicketMap(int size) {
    for (int i = 1; i < size; i++) {
        ticketMap[i] = 1;
    }
}

void *clientHandler(void *param) {
    struct args *client = (struct args *) param;

    /* wait for client arrival time  */
    usleep(client->arrivalTime * 1000);

    string clientName = client->clientName;
    int seatNumber = client->seatNumber;
    string printMsg;
    string tellerName;
    int bufferPointer;

    /* wait until a teller is available */
    sem_wait(&full);

    /* find which teller is available and get its name */
    if (buffer[0] == 1) {
        tellerName = "A";
        bufferPointer = 0;
    } else if (buffer[1] == 1) {
        tellerName = "B";
        bufferPointer = 1;
    } else if (buffer[2] == 1) {
        tellerName = "C";
        bufferPointer = 2;
    }

    /* convert the teller to busy state as it services to this client */
    buffer[bufferPointer] = 0;

    if (ticketMap[seatNumber] == 1) { /* if the client can get the seat he/she wants */
        ticketMap[seatNumber] = 0;
        printMsg = clientName + " requests seat " + to_string(seatNumber) + ", reserves seat " + to_string(seatNumber)
                   + ". Signed by Teller " + tellerName + ".";
    } else { /* if the wanted seat unavailable  */
        bool isSeated = false;
        for (int i = 1; i < totalTicketNumber; ++i) { /* find the smallest number of seat and give it to client  */
            if (ticketMap[i] == 1) {
                ticketMap[i] = 0;
                printMsg = clientName + " requests seat " + to_string(seatNumber) + ", reserves seat " + to_string(i)
                           + ". Signed by Teller " + tellerName + ".";
                isSeated = true;
                break;
            }
        }
        if (!isSeated) { /* if there is no available seat  */
            printMsg = clientName + " requests seat " + to_string(seatNumber) + ", reserves None. Signed by Teller "
                       + tellerName + ".";

        }
    }

    /* service time of the teller */
    usleep(client->serviceTime * 1000);

    /* critical section - get lock to write the message to the output file */
    pthread_mutex_lock(&mutexFile);
    outputFile.open(outputPath, ios::app);
    outputFile << printMsg << endl;
    outputFile.close();

    /* end of critical section unlock */
    pthread_mutex_unlock(&mutexFile);

    /* after the teller served, make it available again  */
    buffer[bufferPointer] = 1;

    /* increase the number of available teller */
    sem_post(&full);

    /* end of thread */
    pthread_exit(NULL);
}

void *tellerHandler(void *param) {
    pthread_t self = pthread_self();
    string msg;

    /* get message of the teller */
    if (self == tid[0]) {
        msg = "Teller A has arrived.";
    } else if (self == tid[1]) {
        msg = "Teller B has arrived.";
    } else if (self == tid[2]) {
        msg = "Teller C has arrived.";
    }

    /* get semaphore */
    sem_wait(&full);

    /* make tellers available */
    buffer[0] = 1;
    buffer[1] = 1;
    buffer[2] = 1;

    /* critical section - write to output file */
    pthread_mutex_lock(&mutexFile);
    outputFile.open(outputPath, ios::app);
    outputFile << msg << endl;
    outputFile.close();
    pthread_mutex_unlock(&mutexFile);

    sem_post(&full);

    pthread_exit(0);
}

