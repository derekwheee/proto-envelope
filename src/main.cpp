#include <Arduino.h>
#include "SAMDTimerInterrupt.h"
#include <Adafruit_DotStar.h>

#define SAMPLE_RATE 6000
#define USING_TIMER_TC3 true
#define SELECTED_TIMER TIMER_TC3
#define TIMER_INTERVAL_US 100000 / SAMPLE_RATE

// Constants
#define ANALOG_HIGH 1023.0
#define DAC_HIGH 4095.0

// Define scaling ranges
const double dacRange[2] = {0, DAC_HIGH};
const double sustainRange[2] = {0, 1};

// DotStar config
#define NUM_LEDS 1
#define DS_DATA_PIN 8
#define DS_CLCK_PIN 6

// Define input/output pins
#define GATE_PIN A5
#define CV_OUT_PIN A0

int gateState = LOW;
int envelopeStage = 0;
int attackCounter = 0;
int decayCounter = 0;
int releaseCounter = 0;

float attackTime = 0.0;
float decayTime = 0.0;
float sustainLevel = 0.0;
float releaseTime = 0.0;

SAMDTimer ITimer(SELECTED_TIMER);
Adafruit_DotStar pixel(NUM_LEDS, DS_DATA_PIN, DS_CLCK_PIN, DOTSTAR_BRG);

void setup()
{
    ITimer.attachInterruptInterval(TIMER_INTERVAL_US, handleInterval);

    pixel.begin();
    pixel.show();

    Serial.begin(9600);
}

void loop()
{
    // Create envelope array based on pot values
}

void handleInterval()
{
    int gateValue = digitalRead(GATE_PIN);

    if (gateValue == HIGH && gateState == LOW)
    {
        gateState = HIGH;
        triggerEnvelope();
    }
    else if (gateValue == LOW)
    {
        gateState = LOW;
        envelopeStage = -1;
    }

    generateEnvelope(envelopeStage);
    incrementEnvelope(envelopeStage);
}

void triggerEnvelope()
{
    envelopeStage = 0;
    attackCounter = 0;
    decayCounter = 0;
    releaseCounter = 0;
}

void incrementEnvelope(int stage)
{
    switch (stage)
    {
    case 0: // ATTACK
        if (attackCounter >= attackTime * SAMPLE_RATE)
        {
            ++stage;
            attackCounter = 0;
        }
        else
        {
            ++attackCounter;
        }
        break;
    case 1: // DECAY
        if (decayCounter >= decayTime * SAMPLE_RATE)
        {
            ++stage;
            decayCounter = 0;
        }
        else
        {
            ++decayCounter;
        }
        break;
    case 2: // SUSTAIN
        // no-op?
        break;
    case 3: // RELEASE
        if (releaseCounter >= releaseTime * SAMPLE_RATE)
        {
            stage = 0;
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

void generateEnvelope(int stage)
{
    double output;
    float sustainLimit = sustainLevel * DAC_HIGH;

    if (stage == -1) {
        output = 0;
    }
    else if (stage == 0)
    {
        output = attackTime == 0 ? DAC_HIGH : attackCounter / attackTime * SAMPLE_RATE * DAC_HIGH;
    }
    else if (stage == 1)
    {
        output = decayTime == 0 ? sustainLimit : (1 - decayCounter / decayTime * SAMPLE_RATE) * (DAC_HIGH - sustainLimit) + sustainLimit;
    }
    else if (stage == 2)
    {
        output = scale(sustainLevel, sustainRange, dacRange);
    }
    else if (stage == 3)
    {
        output = releaseTime == 0 ? 0 : (1 - releaseCounter / releaseTime * SAMPLE_RATE) * (sustainLimit);
    }

    analogWrite(CV_OUT_PIN, output);
}


float scale(float value, const double r1[2], const double r2[2])
{
    return (value - r1[0]) * (r2[1] - r2[0]) / (r1[1] - r1[0]) + r2[0];
}