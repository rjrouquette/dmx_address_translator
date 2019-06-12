//
// Created by robert on 2/12/19.
//

#include <cstring>
#include <iostream>
#include "DmxOut.h"

DmxOut::DmxOut(const std::string &did) :
        deviceId(did)
{
    mutexOutput = PTHREAD_MUTEX_INITIALIZER;
    condOutput = PTHREAD_COND_INITIALIZER;
    ftdi_init(&ftdiContext);
    isRunning = false;
    joinOutput = false;
    blockSize = 0;
    head = 0;
    tail = 0;
}

DmxOut::~DmxOut() {
    stopTx();
    ftdi_deinit(&ftdiContext);
}

bool DmxOut::connect() {
    auto res = ftdi_usb_open_string(&ftdiContext, deviceId.c_str());
    if(res != 0) return false;

    return true;
}

void DmxOut::startTx() {
    isRunning = true;
    joinOutput = true;
    pthread_create(&threadOutput, nullptr, doOutput, this);
}

void DmxOut::stopTx() {
    isRunning = false;
    if(joinOutput) {
        pthread_mutex_lock(&mutexOutput);
        pthread_cond_signal(&condOutput);
        pthread_mutex_unlock(&mutexOutput);
        pthread_join(threadOutput, nullptr);
        joinOutput = false;
    }
}

void* DmxOut::doOutput(void *obj) {
    ((DmxOut *)obj)->doOutput();
    return nullptr;
}

void DmxOut::doOutput() {
    char tname[32];
    sprintf(tname, "dmxOut %s", deviceId.c_str() + deviceId.length() - 3);
    pthread_setname_np(pthread_self(), tname);

    pthread_mutex_lock(&mutexOutput);
    while(isRunning) {
        while(isRunning && tail == head) {
            pthread_cond_wait(&condOutput, &mutexOutput);
        }
        pthread_mutex_unlock(&mutexOutput);
        while(tail != head) {
            sendBlock(dmxBlock[tail]);
            tail = (tail + 1) & 15;
        }
        pthread_mutex_lock(&mutexOutput);
    }
    pthread_mutex_unlock(&mutexOutput);
}

void DmxOut::sendBlock(int len, uint8_t *block) {
    uint8_t *nextBlock = dmxBlock[head];
    memcpy(nextBlock, block, (size_t)std::min(len, blockSize));
    for(auto &t : translations) {
        nextBlock[t.dst] = block[t.src];
    }
    head = (head + 1) & 15;
    pthread_mutex_lock(&mutexOutput);
    pthread_cond_signal(&condOutput);
    pthread_mutex_unlock(&mutexOutput);
}

void DmxOut::sendBlock(uint8_t *block) {
    // send preamble
    uint8_t zero = 0;
    ftdi_set_line_property(&ftdiContext, BITS_8, STOP_BIT_2, NONE);
    ftdi_set_baudrate(&ftdiContext, 57600);
    ftdi_write_data(&ftdiContext, &zero, 1);

    // send frame data
    ftdi_set_line_property(&ftdiContext, BITS_8, STOP_BIT_2, NONE);
    ftdi_set_baudrate(&ftdiContext, 250000);
    ftdi_write_data(&ftdiContext, block, blockSize);
}

void DmxOut::setBlockSize(int size) {
    blockSize = size;
}

void DmxOut::clearTranslations() {
    translations.clear();
}

void DmxOut::addTranslation(int src, int dest) {
    translations.emplace_back(src, dest);
}
