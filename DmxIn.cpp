//
// Created by robert on 2/12/19.
//

#include <iostream>
#include "DmxIn.h"

DmxIn::DmxIn(const std::string &did) :
        deviceId(did)
{
    ftdi_init(&ftdiContext);
    isRunning = false;
    joinInput = false;
    ref = nullptr;
    cb = nullptr;
}

DmxIn::~DmxIn() {
    stopRx();
    ftdi_deinit(&ftdiContext);
}

bool DmxIn::connect() {
    auto res = ftdi_usb_open_string(&ftdiContext, deviceId.c_str());
    if(res != 0) return false;

    ftdi_set_line_property(&ftdiContext, BITS_8, STOP_BIT_2, NONE);
    ftdi_set_baudrate(&ftdiContext, 250000);
    return true;
}

void DmxIn::startRx() {
    isRunning = true;
    joinInput = true;
    pthread_create(&inputThread, nullptr, doInput, this);
}

void DmxIn::stopRx() {
    isRunning = false;
    if(joinInput) {
        pthread_join(inputThread, nullptr);
        joinInput = false;
    }
}

void* DmxIn::doInput(void *obj) {
    ((DmxIn *)obj)->doInput();
    return nullptr;
}

void DmxIn::doInput() {
    char tname[32];
    sprintf(tname, "dmxIn %s", deviceId.c_str() + deviceId.length() - 3);
    pthread_setname_np(pthread_self(), tname);

    uint8_t buff, dmxBlock[1024];
    int idx = 0;

    auto prevRx = microRT();
    while(isRunning) {
        ftdi_read_data(&ftdiContext, &buff, 1);

        auto now = microRT();
        int64_t delta = now - prevRx;
        prevRx = now;
        if(delta > 80) {
            if(idx > 1 && dmxBlock[0] == 0 && cb) {
                (*cb)(ref, idx, dmxBlock);
            }
            idx = 0;
        }
        dmxBlock[idx++] = buff;
        idx &= 1023;
    }
}

uint64_t DmxIn::microRT() {
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    auto res = (uint64_t) ts.tv_nsec;
    res /= 1000;
    res += ((uint64_t)ts.tv_sec) * 1000000;
    return res;
}
