#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <json-c/json.h>
#include <libftdi1/ftdi.h>
#include <sysexits.h>
#include <cstring>
#include <set>
#include "DmxIn.h"
#include "DmxOut.h"

pthread_t threadProcess;
pthread_mutex_t mutexProcess;
pthread_cond_t condProcess;

bool isRunning = true;
uint16_t outputCnt = 0;
int head = 0, tail = 0;
int dmxBlockLen[16];
uint8_t dmxBlock[16][1024];
uint64_t lastBlock;

std::list<DmxOut> outputs;
std::set<int> channels;

json_object* loadJSON(const char* file);
json_object* parseJSON(const char* str, int len);

void dmxRX(void *ref, int len, uint8_t *block);

void * doProcessing(void *obj);

static uint64_t microRT();

int main(int argc, char **argv) {
    json_object *jtmp, *jtmp2, *jouts, *jout;

    std::cout << "loading config " << argv[1] << std::endl;

    auto config = loadJSON(argv[1]);
    if(config == nullptr) {
        std::cout << "failed to load config " << argv[1] << std::endl;
        return EX_CONFIG;
    }

    json_object_object_get_ex(config, "input", &jtmp);
    json_object_object_get_ex(jtmp, "deviceId", &jtmp2);
    DmxIn dmxIn(json_object_get_string(jtmp2));
    std::cout << "DMX Input: " << json_object_get_string(jtmp2) << std::endl;

    json_object_object_get_ex(config, "outputs", &jouts);
    for(int i = 0; i < json_object_array_length(jouts); i++) {
        jout = json_object_array_get_idx(jouts, i);
        json_object_object_get_ex(jout, "deviceId", &jtmp);
        outputs.emplace_back(json_object_get_string(jtmp));

        std::cout << "DMX Output: " << json_object_get_string(jtmp);

        auto &out = outputs.back();

        json_object_object_get_ex(jout, "blockSize", &jtmp);
        out.setBlockSize(json_object_get_int(jtmp));
        std::cout << " [" << json_object_get_int(jtmp) << "]";

        json_object_object_get_ex(jout, "translations", &jtmp);
        for(int j = 0; j < json_object_array_length(jtmp); j++) {
            jtmp2 = json_object_array_get_idx(jtmp, j);
            out.addTranslation(
                    json_object_get_int(json_object_array_get_idx(jtmp2, 0)),
                    json_object_get_int(json_object_array_get_idx(jtmp2, 1))
            );
            channels.insert(json_object_get_int(json_object_array_get_idx(jtmp2, 0)));
            std::cout << " ; " << json_object_get_int(json_object_array_get_idx(jtmp2, 0));
            std::cout << " -> " << json_object_get_int(json_object_array_get_idx(jtmp2, 1));
        };
        std::cout << std::endl;
    }

    pthread_create(&threadProcess, nullptr, doProcessing, nullptr);
    std::cout << "Hand-off thread started." << std::endl;

    for(auto &out : outputs) {
        out.connect();
        out.startTx();
    }
    std::cout << "Output threads started." << std::endl;

    dmxIn.connect();
    dmxIn.setCallback(dmxRX);

    char answer[32];
    std::cout << "Start input thread? [y/n]  " << std::flush;
    std::cin >> answer;

    if(answer[0] == 'y' || answer[0] == 'Y') {
        lastBlock = microRT();
        dmxIn.startRx();
        pause();
        std::cout << "Stopping input thread." << std::endl;
        dmxIn.stopRx();
    }

    isRunning = false;
    pthread_mutex_lock(&mutexProcess);
    pthread_cond_signal(&condProcess);
    pthread_mutex_unlock(&mutexProcess);
    std::cout << "Stopping hand-off thread." << std::endl;
    pthread_join(threadProcess, nullptr);

    std::cout << "Stopping output threads." << std::endl;
    outputs.clear();

    return 0;
}

void dmxRX(void *ref, int len, uint8_t *block) {
    dmxBlockLen[head] = len;
    memcpy(dmxBlock[head], block, (size_t)len);
    head = (head + 1) & 15;
    pthread_mutex_lock(&mutexProcess);
    pthread_cond_signal(&condProcess);
    pthread_mutex_unlock(&mutexProcess);
}

static int prevBlockSize = 0;

void printData(const uint8_t *block, int blockSize) {
    uint64_t now = microRT();
    int64_t delta = now - lastBlock;
    lastBlock = now;

    printf("%04x: ", outputCnt++);
    for(auto i : channels) {
        int tcolor = ((22 * (uint16_t)block[i]) / 255) + 233;
        printf("\033[38;5;%dm%02x\033[0m ", tcolor, block[i]);
    }
    if(blockSize == prevBlockSize) {
        printf("|  %0.2f ms \033[32;1m[%d]\033[0m", delta / 1000.0, blockSize);
    }
    else {
        printf("|  %0.2f ms \033[31;1m[%d]\033[0m", delta / 1000.0, blockSize);
    }
    std::cout << std::endl;
    prevBlockSize = blockSize;
}

void * doProcessing(void *obj) {
    pthread_mutex_lock(&mutexProcess);
    while(isRunning) {
        while(isRunning && tail == head) {
            pthread_cond_wait(&condProcess, &mutexProcess);
        }
        pthread_mutex_unlock(&mutexProcess);
        while(tail != head) {
            for(auto &out : outputs) {
                out.sendBlock(dmxBlockLen[tail], dmxBlock[tail]);
            }
            printData(dmxBlock[tail], dmxBlockLen[tail]);
            tail = (tail + 1) & 15;
        }
        pthread_mutex_lock(&mutexProcess);
    }
    pthread_mutex_unlock(&mutexProcess);
    return nullptr;
}

json_object* loadJSON(const char* file) {
    int fsock = open(file, O_RDONLY);
    if (fsock == -1) {
        return nullptr;
    }

    struct stat sb = {};
    if (fstat(fsock, &sb) == -1) {
        close(fsock);
        return nullptr;
    }
    void* fmap = mmap(nullptr, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fsock, 0);
    if (fmap == MAP_FAILED) {
        close(fsock);
        return nullptr;
    }
    close(fsock);

    json_object *obj = parseJSON((char*)fmap, (int)sb.st_size);
    munmap(fmap, (size_t)sb.st_size);

    return obj;
}

json_object* parseJSON(const char* str, int len) {
    struct json_tokener* tok;
    struct json_object* obj;

    tok = json_tokener_new_ex(16);
    if (!tok) return nullptr;
    json_tokener_set_flags(tok, JSON_TOKENER_STRICT);

    obj = json_tokener_parse_ex(tok, str, len);
    if(json_tokener_get_error(tok) == json_tokener_continue) {
        obj = json_tokener_parse_ex(tok, "\0", 1);
    }

    if(json_tokener_get_error(tok) != json_tokener_success) {
        if (obj != nullptr) json_object_put(obj);
        obj = nullptr;
    }

    json_tokener_free(tok);

    return obj;
}

uint64_t microRT() {
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    auto res = (uint64_t) ts.tv_nsec;
    res /= 1000;
    res += ((uint64_t)ts.tv_sec) * 1000000;
    return res;
}
