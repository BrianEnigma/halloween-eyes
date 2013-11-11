// # vim:expandtab:shiftwidth=4:tabstop=4:smarttab:autoindent:autoindent:syntax=cpp
// Halloween Eyes
// Brian Enigma, <brian@netninja.com>
// http://nja.me/eyes
//
// Based in part on the "Roboface" code from Adafruit:
// https://github.com/adafruit/Adafruit-LED-Backpack-Library/blob/master/examples/roboface/roboface.pde
//
// ----------------------------------------
//
// Adafruit invests time and resources providing this open source code,
// please support Adafruit and open-source hardware by purchasing
// products from Adafruit!
//
// Written by P. Burgess for Adafruit Industries.
// BSD license, all text above must be included in any redistribution.

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#define MOTION_LED 13
#define LEFT_EYE  0
#define RIGHT_EYE 1
Adafruit_BicolorMatrix eyeMatrix[2];

// Rather than assigning matrix addresses sequentially in a loop, each
// has a spot in this array.  This makes it easier if you inadvertently
// install one or more matrices in the wrong physical position --
// re-order the addresses in this table and you can still refer to
// matrices by index above, no other code or wiring needs to change.
static const uint8_t matrixAddr[] = { 0x70, 0x71 };

static const uint8_t PROGMEM // Bitmaps are stored in program memory
blinkImg[][8] = {    // Eye animation frames
    // The NICE (green) eye, both left and right
    { B00111100,         // Fully open nice/green eye
        B01111110,
        B11111111,
        B11111111,
        B11111111,
        B11111111,
        B01111110,
        B00111100 },
    { B00000000,
        B01111110,
        B11111111,
        B11111111,
        B11111111,
        B11111111,
        B01111110,
        B00111100 },
    { B00000000,
        B00000000,
        B00111100,
        B11111111,
        B11111111,
        B11111111,
        B00111100,
        B00000000 },
    { B00000000,
        B00000000,
        B00000000,
        B00111100,
        B11111111,
        B01111110,
        B00011000,
        B00000000 },
    { B00000000,         // Fully closed nice/green eye
        B00000000,
        B00000000,
        B00000000,
        B10000001,
        B01111110,
        B00000000,
        B00000000 },

    // The EVIL (red) eye, left
    { B00000000,         // Fully open evil/red eye
        B11000000,
        B11111000,
        B01111111,
        B01111111,
        B00111111,
        B00111110,
        B00011100 },
    { B00000000,
        B00000000,
        B11111000,
        B01111111,
        B01111111,
        B00111111,
        B00111110,
        B00011100 },
    { B00000000,
        B00000000,
        B11111000,
        B01111111,
        B01111111,
        B00111111,
        B00111110,
        B00000000 },
    { B00000000,
        B00000000,
        B00000000,
        B01111111,
        B01111111,
        B00111111,
        B00000000,
        B00000000 },
    { B00000000,         // Fully closed evil/red eye
        B00000000,
        B00000000,
        B00000000,
        B01111111,
        B00000000,
        B00000000,
        B00000000 },
    // The EVIL (red) eye, right
    { B00000000,         // Fully open evil/red eye
        B00000011,
        B00011111,
        B11111110,
        B11111110,
        B11111100,
        B01111100,
        B00111000 },
    { B00000000,
        B00000000,
        B00011111,
        B11111110,
        B11111110,
        B11111100,
        B01111100,
        B00111000 },
    { B00000000,
        B00000000,
        B00011111,
        B11111110,
        B11111110,
        B11111100,
        B01111100,
        B00000000 },
    { B00000000,
        B00000000,
        B00000000,
        B11111110,
        B11111110,
        B11111100,
        B00000000,
        B00000000 },
    { B00000000,         // Fully closed evil/red eye
        B00000000,
        B00000000,
        B00000000,
        B11111110,
        B00000000,
        B00000000,
        B00000000 }
};

#define EVIL_EYE_OFFSET (8 * 5)

uint8_t
blinkIndex[] = { 1, 2, 3, 4, 3, 2, 1 }, // Blink bitmap sequence
    blinkCountdown = 100, // Countdown to next blink (in frames)
    gazeCountdown  =  75, // Countdown to next eye movement
    gazeFrames     =  50; // Duration of eye movement (smaller = faster)
int8_t
    eyeX = 3, eyeY = 3,   // Current eye position
    newX = 3, newY = 3,   // Next eye position
    dX   = 0, dY   = 0;   // Distance from prior to new position

void setup() {
    // Seed random number generator from an unused analog input:
    randomSeed(analogRead(A0));

    pinMode(MOTION_LED, OUTPUT);
    digitalWrite(MOTION_LED, LOW);
    pinMode(A3, INPUT);
    digitalWrite(A3, LOW);

    eyeMatrix[LEFT_EYE ].begin(matrixAddr[LEFT_EYE]);
    eyeMatrix[RIGHT_EYE].begin(matrixAddr[RIGHT_EYE]);
    // If using 'small' (1.2") displays vs. 'mini' (0.8"), enable this:
    eyeMatrix[LEFT_EYE ].setRotation(3);
    eyeMatrix[RIGHT_EYE].setRotation(3);
}

int blinkColorSwitchCounter = 0;
int8_t eyeColor = LED_GREEN;
int8_t eyeOffset = 0;

void loop() 
{
    uint8_t motion;
    const uint8_t *imageOffset;
    // Draw eyeball in current state of blinkyness (no pupil).  Note that
    // only one eye needs to be drawn.  Because the two eye matrices share
    // the same address, the same data will be received by both.
    eyeMatrix[LEFT_EYE ].clear();
    eyeMatrix[RIGHT_EYE].clear();

    // When counting down to the next blink, show the eye in the fully-
    // open state.  On the last few counts (during the blink), look up
    // the corresponding bitmap index.
    imageOffset = 
            &blinkImg[
            (blinkCountdown < sizeof(blinkIndex)) ? // Currently blinking?
            blinkIndex[blinkCountdown] :            // Yes, look up bitmap #
            0                                       // No, show bitmap 0
            ][0] + eyeOffset;
    eyeMatrix[LEFT_EYE ].drawBitmap(0, 0, imageOffset, 8, 8, eyeColor);
    if (LED_RED == eyeColor)
        imageOffset += EVIL_EYE_OFFSET;
    eyeMatrix[RIGHT_EYE].drawBitmap(0, 0, imageOffset, 8, 8, eyeColor);

    // Decrement blink counter.  At end, set random time for next blink.
    blinkCountdown--;
    if (2 == blinkCountdown)
        blinkColorSwitchCounter++;
    if (blinkCountdown == 0) 
        blinkCountdown = random(5, 180);

    // Switch eye color mid-blink
    if (blinkColorSwitchCounter >= 5)
    {
        eyeColor = (LED_GREEN == eyeColor) ? LED_RED : LED_GREEN;
        eyeOffset = (LED_GREEN == eyeColor) ? 0 : EVIL_EYE_OFFSET;
        blinkColorSwitchCounter = 0;
        if (LED_RED == eyeColor)
        {
            eyeX = 4;
            eyeY = 4;
            newX = 4;
            newY = 4;
            dX = 0;
            dY = 0;
        }
    }

    // Add a pupil (2x2 black square) atop the blinky eyeball bitmap.
    // Periodically, the pupil moves to a new position...
    if(--gazeCountdown <= gazeFrames) {
        // Eyes are in motion - draw pupil at interim position
        eyeMatrix[LEFT_EYE].fillRect(
                newX - (dX * gazeCountdown / gazeFrames),
                newY - (dY * gazeCountdown / gazeFrames),
                2, 2, LED_OFF);
        eyeMatrix[RIGHT_EYE].fillRect(
                newX - (dX * gazeCountdown / gazeFrames) - (LED_RED == eyeColor ? 2 : 0),
                newY - (dY * gazeCountdown / gazeFrames),
                2, 2, LED_OFF);
        if(gazeCountdown == 0) {    // Last frame?
            eyeX = newX; eyeY = newY; // Yes.  What's new is old, then...
            if (LED_RED == eyeColor)
            {
                newY = 4;
                newX = random(3) + 3;
            }
            else 
            {
                do { // Pick random positions until one is within the eye circle
                    newX = random(7); newY = random(7);
                    dX   = newX - 3;  dY   = newY - 3;
                } while((dX * dX + dY * dY) >= 10);      // Thank you Pythagoras
            }
            dX            = newX - eyeX;             // Horizontal distance to move
            dY            = newY - eyeY;             // Vertical distance to move
            gazeFrames    = random(3, 15);           // Duration of eye movement
            gazeCountdown = random(gazeFrames, 120); // Count to end of next movement
        }
    } else {
        // Not in motion yet -- draw pupil at current static position
        eyeMatrix[LEFT_EYE ].fillRect(eyeX, eyeY, 2, 2, LED_OFF);
        eyeMatrix[RIGHT_EYE].fillRect(eyeX - (LED_RED == eyeColor ? 2 : 0), eyeY, 2, 2, LED_OFF);
    }

    // Refresh all of the matrices in one quick pass
    eyeMatrix[LEFT_EYE ].writeDisplay();
    eyeMatrix[RIGHT_EYE].writeDisplay();

    // Check for motion. If motion, set to red eyes on the next blink.
    // If still in motion, disallow it to switch back to green
    motion = HIGH == digitalRead(A3);
    digitalWrite(MOTION_LED, motion);
    if (motion)
    {
        if (LED_GREEN == eyeColor)
        {
            if (blinkCountdown > 20)
                blinkCountdown = 20;
            blinkColorSwitchCounter = 4; // Switch on next blink
        } else
            blinkColorSwitchCounter = 0; // Hold off on switching
    } else {
        if (LED_GREEN == eyeColor)
            blinkColorSwitchCounter = 0; // Hold off on switching
    }
    delay(20); // ~50 FPS
}

