//
// Created by robert on 2/12/19.
//

#ifndef DMX_ADDRESS_TRANSLATOR_DMXIN_H
#define DMX_ADDRESS_TRANSLATOR_DMXIN_H

#include <libftdi1/ftdi.h>
#include <pthread.h>
#include <string>

class DmxIn {
public:
    typedef void (*callback)(void *ref, int len, uint8_t *dmxBlock);

private:
    const std::string deviceId;
    ftdi_context ftdiContext;
    pthread_t inputThread;
    bool isRunning, joinInput;
    void *ref;
    callback cb;

    static uint64_t microRT();
    static void * doInput(void *obj);
    void doInput();

public:

    DmxIn(const std::string &deviceId);
    ~DmxIn();

    bool connect();
    void startRx();
    void stopRx();

    void setReference(void *ref) { this->ref = ref; }
    void setCallback(callback cb) { this->cb = cb; }
};


#endif //DMX_ADDRESS_TRANSLATOR_DMXIN_H
