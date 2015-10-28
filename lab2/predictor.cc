#include "predictor.h"
#include "assert.h"
#include "math.h"

#define STRONGLY_NOT_TAKEN 0
#define WEAKLY_NOT_TAKEN 1
#define WEAKLY_TAKEN 2
#define STRONGLY_TAKEN 3


/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

static int predictionTable [4096];

UINT32 getPredictionIndex(UINT32 PC) {
    return PC & 0xfff;   
}

// Lab 2 - Custom Code
// get prediction from a 2bit saturating counter which is a 1d array with a value of 0-3 defined at the top
bool makePrediction(UINT32 index, int* table) {

    switch(table[index]){
        case STRONGLY_NOT_TAKEN:
        case WEAKLY_NOT_TAKEN:
            return NOT_TAKEN;
        case WEAKLY_TAKEN:
        case STRONGLY_TAKEN:
            return TAKEN;
        default:
            //This is an unexpected case
            assert(false && "We should not be entering this case");  //Adding a default case to remove warning
            return 0;
    };
}

//update a 2 bit prediction counter which is a 1d array with a value of 0-3 defined at the top
void updatePrediction(bool resolveDir, UINT32 index, int* table) {
    
    if(NOT_TAKEN == resolveDir &&
       STRONGLY_NOT_TAKEN != table[index]){
        table[index]--;
    }
    else if(TAKEN == resolveDir &&
       STRONGLY_TAKEN != table[index]){
        table[index]++;
    }   
}

void InitPredictor_2bitsat() {
    int i;
    //initialize entire prediction table to weakly not taken
    for(i = 0; i < 4096; i++){
        predictionTable[i] = WEAKLY_NOT_TAKEN;
    }
}

bool GetPrediction_2bitsat(UINT32 PC) {

    UINT32 index = getPredictionIndex(PC);

    return makePrediction(index, predictionTable);

}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

    UINT32 index = getPredictionIndex(PC);   

    updatePrediction(resolveDir, index, predictionTable);

}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

static int bht[512];
static int pht[8][64];

UINT32 getBHTIndex(UINT32 PC) {
    return PC & 0xff8 >> 3;   
}

UINT32 getPHTIndex(UINT32 PC) {
    return PC & 0x7;   
}

int get_6bit_history(UINT32 index) {
    return bht[index] & 0x3f;
}

//get a prediction from the 2 level predictor
int make_two_level_prediction(UINT32 PC) {

    //get table addresses
    UINT32 bht_index = getBHTIndex(PC);

    UINT32 pht_row = getPHTIndex(PC);
    UINT32 pht_column = get_6bit_history(bht_index);

    return makePrediction(pht_column, pht[pht_row]);

}

// update a Branch history table
void record_history(UINT32 index, bool resolveDir, int* table) {
    table[index] = table[index] << 1 | resolveDir;
}

// update the 2 level predictor
void update_two_level_prediction(UINT32 PC, bool resolveDir) {

    // get table addresses
    UINT32 bht_index = getBHTIndex(PC);

    UINT32 pht_row = getPHTIndex(PC);
    UINT32 pht_column = get_6bit_history(bht_index);

    // update the pht
    updatePrediction(resolveDir, pht_column, pht[pht_row]);

    //update the bht
    record_history(bht_index, resolveDir, bht);
}

void InitPredictor_2level() {
    int i;

    int* tmp_pht = (int*)pht; // temperary pointer to the pht block of memory

    for (i = 0; i < 512; i++){
        bht[i] = 0;
        *tmp_pht++ = WEAKLY_NOT_TAKEN;
    }
}

bool GetPrediction_2level(UINT32 PC) {
    return make_two_level_prediction(PC);
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    update_two_level_prediction(PC, resolveDir);
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

/***********************************************************
 * ONE Level predictor constants and structures
 **********************************************************/
const int HISTORY_1LVL_SIZE = 12;
const int PHT_1LVL_SIZE = pow(2, HISTORY_1LVL_SIZE);
const UINT32 bitmask_1lvl = 0xfff;
const UINT32 offset_1lvl = 0;

static int oe_1lvl_bht;
static int oe_1lvl_pht[PHT_1LVL_SIZE];

/*********************************************************
 * TWO Level predictor constants and structures
 * ******************************************************/

const int HISTORY_2LVL_SIZE = 6;
const int BHT_2LVL_SIZE = 512;
const int PHT_2LVL_COL = pow(2, HISTORY_2LVL_SIZE);
const int PHT_2LVL_ROW = 8;
const UINT32 BHT_2LVL_MASK = 0xff8;
const UINT32 BHT_2LVL_OFFSET = 3;

const UINT32 PHT_2LVL_MASK = 0x7;
const UINT32 PHT_2LVL_OFFSET = 0;

const UINT32 HISTORY_MASK = 0x3f;
const UINT32 HISTORY_OFFSET = 0;

static int oe_2lvl_bht[BHT_2LVL_SIZE];
static int oe_2lvl_pht[PHT_2LVL_ROW][PHT_2LVL_COL];

/*********************************************************
 * Predictor Selector 
 * ******************************************************/

/********* ENUM DEFINITION OF ALL OUR PREDICTORS ********/ 
enum {
    ONELVL,
    TWOLVL,
    NUM_PREDICTORS
};

typedef struct {
    int streak[NUM_PREDICTORS];
    int prediction[NUM_PREDICTORS];
}StreakTracker;

static StreakTracker strk;

//Generic funtion to get index for our BHT and PHT tables
UINT32 maskAndShift(UINT32 src, UINT32 bitmask, UINT32 rightOffset) {
    return src & bitmask >> rightOffset;   
}

/******************************************** 
 * OUR PREDICTOR FUNCTIONS 
 * *****************************************/

/********** 1 LVL PREDICTOR ***************/
int Get1LvlPrediction(UINT32 PC) {
    UINT32 pht_index = maskAndShift(oe_1lvl_bht, bitmask_1lvl, offset_1lvl);
    return makePrediction(pht_index, oe_1lvl_pht);
}

void Update1LvlPrediction(UINT32 PC, bool resolveDir) {
    UINT32 pht_index = maskAndShift(oe_1lvl_bht, bitmask_1lvl, offset_1lvl);
    updatePrediction(resolveDir, pht_index, oe_1lvl_pht);
    //Update history bits of GLobal History Branch 
    oe_1lvl_bht = oe_1lvl_bht << 1 | resolveDir;
}

/********** 2 LVL PREDICTOR **************/

int Get2LvlPrediction(UINT32 PC){
    UINT32 bht_index = maskAndShift(PC, BHT_2LVL_MASK, BHT_2LVL_OFFSET);
    UINT32 pht_row = maskAndShift(PC, PHT_2LVL_MASK, PHT_2LVL_OFFSET);
    
    UINT32 pht_col = maskAndShift(oe_2lvl_bht[bht_index], HISTORY_MASK, HISTORY_OFFSET);

    return makePrediction(pht_col, oe_2lvl_pht[pht_row]);
}

void Update2LvlPrediction(UINT32 PC, bool resolveDir){ 
    UINT32 bht_index = maskAndShift(PC, BHT_2LVL_MASK, BHT_2LVL_OFFSET);
    UINT32 pht_row = maskAndShift(PC, PHT_2LVL_MASK, PHT_2LVL_OFFSET);
    
    UINT32 pht_col = maskAndShift(oe_2lvl_bht[bht_index], HISTORY_MASK, HISTORY_OFFSET);
    
    updatePrediction(resolveDir, pht_col, oe_2lvl_pht[pht_row]);
    
    record_history(bht_index, resolveDir, oe_2lvl_bht);
}

/********** PREDICTOR CHOOSER *************/
int choosePrediction(UINT32 PC) {
    /******* ADD NEW PREDICTOR OPERATIONS HERE *********/
    strk.prediction[ONELVL] = Get1LvlPrediction(PC);
    strk.prediction[TWOLVL] = Get2LvlPrediction(PC);

    int max_index = 0;

    int i = 0;
    for(; i < NUM_PREDICTORS; i++){
        if(strk.streak[i] > strk.streak[max_index]){
            max_index = i;
        }
    }

    return strk.prediction[max_index];
}

void UpdateChooser(bool resolveDir) {
    int i = 0;
    for(; i < NUM_PREDICTORS; i++){
        if(strk.prediction[i] == (int)resolveDir){
            strk.streak[i]++;
        }
        else{
            strk.streak[i] = 0;
        }
    }
}

void InitPredictor_openend() {
    //Initialize 1-Level Predictor
    oe_1lvl_bht = 0;
    int i;
    for(i = 0; i < PHT_1LVL_SIZE; i++){
        oe_1lvl_pht[i] = WEAKLY_NOT_TAKEN;
    }
    //Initialize 2-Level Predictor    
    for(i = 0; i < BHT_2LVL_SIZE; i++) {
        oe_2lvl_bht[i] = 0;
    }
    
    int *tmp_pht = (int*)oe_2lvl_pht;
    for(i = 0; i < PHT_2LVL_ROW * PHT_2LVL_COL; i++) {
        *tmp_pht++ = WEAKLY_NOT_TAKEN;
    }
}

bool GetPrediction_openend(UINT32 PC) {
  
  return choosePrediction(PC);
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    Update1LvlPrediction(PC, resolveDir);
    Update2LvlPrediction(PC, resolveDir);
    UpdateChooser(resolveDir);
}

