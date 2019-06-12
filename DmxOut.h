//
// Created by robert on 2/12/19.
//

#ifndef DMX_ADDRESS_TRANSLATOR_DMXOUT_H
#define DMX_ADDRESS_TRANSLATOR_DMXOUT_H

#include <libftdi1/ftdi.h>
#include <pthread.h>
#include <string>
#include <list>

class DmxOut {
private:
    struct translation {
        int src;
        int dst;
        translation() { src = 0; dst = 0; }
        translation(int s, int d) { src = s; dst = d; }
    };

    const std::string deviceId;
    ftdi_context ftdiContext;
    bool isRunning, joinOutput;
    pthread_t threadOutput;
    pthread_mutex_t mutexOutput;
    pthread_cond_t condOutput;
    std::list<translation> translations;

    int blockSize, head, tail;
    uint8_t dmxBlock[16][1024];

    static void * doOutput(void *obj);
    void doOutput();
    void sendBlock(uint8_t *block);

public:
    DmxOut(const std::string &deviceId);
    ~DmxOut();

    bool connect();
    void startTx();
    void stopTx();

    void setBlockSize(int size);
    void clearTranslations();
    void addTranslation(int src, int dest);

    void sendBlock(int len, uint8_t *block);
};


#endif //DMX_ADDRESS_TRANSLATOR_DMXOUT_H
