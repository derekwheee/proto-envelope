#include <Arduino.h>
#include <Wire.h>
#include "SAMDTimerInterrupt.h"
#include <Adafruit_DotStar.h>

#define SAMPLE_RATE 6000
#define STAGE_DURATION 10 * SAMPLE_RATE
#define USING_TIMER_TC3 true
#define SELECTED_TIMER TIMER_TC3
#define TIMER_INTERVAL_US 100000 / SAMPLE_RATE

// Constants
#define ANALOG_HIGH 1023.0
#define DAC_HIGH 4095.0

// Define scaling ranges
const double dacRange[2] = {0, DAC_HIGH};
const double voltageRange[2] = {0, ANALOG_HIGH};
const double asdrRange[2] = {0, 1};
const double sustainRange[2] = {0, 1};

// DotStar config
#define NUM_LEDS 1
#define DS_DATA_PIN 8
#define DS_CLCK_PIN 6

// Define input/output pins
#define GATE_PIN 7
#define CV_OUT_PIN A1
#define ATTACK_PIN A5
#define DECAY_PIN A4
#define SUSTAIN_PIN A3
#define RELEASE_PIN A2

enum ENVELOPE_STAGE
{
    INIT = -1,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE
};

ENVELOPE_STAGE envelopeStage = INIT;

int gateState = LOW;
int attackCounter = 0;
int decayCounter = 0;
int releaseCounter = 0;

float attackTime = 0.1;
float decayTime = 0.1;
float sustainLevel = 0.5;
float releaseTime = 0.1;

double envelopeOutput = 0.0;

int gateValue;

SAMDTimer ITimer(SELECTED_TIMER);
Adafruit_DotStar pixel(NUM_LEDS, DS_DATA_PIN, DS_CLCK_PIN, DOTSTAR_BRG);

float scale(float value, const double r1[2], const double r2[2]);
void handleInterval();
void triggerEnvelope();
void generateEnvelope();
void incrementEnvelope();

void setup()
{
    pinMode(GATE_PIN, INPUT_PULLDOWN);

    ITimer.attachInterruptInterval(TIMER_INTERVAL_US, handleInterval);

    pixel.begin();
    pixel.show();

    Serial.begin(115200);
}

void loop()
{
    attackTime = scale(analogRead(ATTACK_PIN), voltageRange, asdrRange);
    decayTime = scale(analogRead(DECAY_PIN), voltageRange, asdrRange);
    sustainLevel = scale(analogRead(SUSTAIN_PIN), voltageRange, asdrRange);
    releaseTime = scale(analogRead(RELEASE_PIN), voltageRange, asdrRange);

    if (Serial)
    {
        Serial.println(envelopeStage);
        Serial.println(envelopeOutput);
        Serial.println(gateValue);
        Serial.println(attackTime);
        Serial.println(decayTime);
        Serial.println(sustainLevel);
        Serial.println(releaseTime);
    }

    pixel.setPixelColor(0, pixel.Color(0, 0, 64 * envelopeOutput / DAC_HIGH));
    pixel.show();

    delay(10);
}

float scale(float value, const double r1[2], const double r2[2])
{
    return (value - r1[0]) * (r2[1] - r2[0]) / (r1[1] - r1[0]) + r2[0];
}

void handleInterval()
{
    gateValue = digitalRead(GATE_PIN);

    if (gateValue == HIGH && gateState == LOW)
    {
        gateState = HIGH;
        triggerEnvelope();
    }
    else if (gateValue == LOW && gateState == HIGH)
    {
        envelopeStage = RELEASE;
        gateState = LOW;
    }
    else if (gateValue == LOW)
    {
        gateState = LOW;
    }

    generateEnvelope();
    incrementEnvelope();
}

void triggerEnvelope()
{
    envelopeStage = ATTACK;
    attackCounter = 0;
    decayCounter = 0;
    releaseCounter = 0;
}

void incrementEnvelope()
{
    switch (envelopeStage)
    {
    case ATTACK:
        if (attackCounter >= attackTime * STAGE_DURATION)
        {
            envelopeStage = DECAY;
            attackCounter = 0;
        }
        else
        {
            ++attackCounter;
        }
        break;
    case DECAY:
        if (decayCounter >= decayTime * STAGE_DURATION)
        {
            envelopeStage = SUSTAIN;
            decayCounter = 0;
        }
        else
        {
            ++decayCounter;
        }
        break;
    case SUSTAIN:
        // no-op?
        break;
    case RELEASE:
        if (releaseCounter >= releaseTime * STAGE_DURATION)
        {
            envelopeStage = INIT;
            releaseCounter = 0;
        }
        else
        {
            ++releaseCounter;
        }
        break;

    default:
        break;
    }
}

void generateEnvelope()
{
    float sustainLimit = sustainLevel * DAC_HIGH;

    switch (envelopeStage)
    {
    case INIT:
        envelopeOutput = 0;
        break;
    case ATTACK:
        envelopeOutput = attackTime == 0 ? DAC_HIGH : attackCounter / (attackTime * STAGE_DURATION) * DAC_HIGH;
        break;
    case DECAY:
        envelopeOutput = decayTime == 0 ? sustainLimit : (1 - decayCounter / (decayTime * STAGE_DURATION)) * (DAC_HIGH - sustainLimit) + sustainLimit;
        break;
    case SUSTAIN:
        envelopeOutput = scale(sustainLevel, sustainRange, dacRange);
        break;
    case RELEASE:
        envelopeOutput = releaseTime == 0 ? 0 : (1 - releaseCounter / (releaseTime * STAGE_DURATION)) * sustainLimit;
        break;
    default:
        break;
    }

    analogWrite(CV_OUT_PIN, envelopeOutput);
}