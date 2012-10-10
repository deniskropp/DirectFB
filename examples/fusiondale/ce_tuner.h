#ifndef __AV_TUNER__
#define __AV_TUNER__


typedef enum {
     AV_TUNER_SETFREQUENCY,
     AV_TUNER_SETGAINS
} AVTunerMethods;

typedef enum {
     AV_TUNER_ONSTATIONFOUND,
     AV_TUNER_ONSTATIONLOST,

     _AV_TUNER_NUM_NOTIFICATIONS
} AVTunerNotifications;


#define AV_TUNER_MAX_GAINS  16

typedef struct {
     int  num;
     int  gains[AV_TUNER_MAX_GAINS];
} AVTunerSetGainsCtx;


#endif

