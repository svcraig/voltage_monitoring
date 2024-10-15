#include <SPI.h>
#include <SD.h>

/* START: The values below can be edited depending on the application. */
// #define DISABLE_SD_CARD                         // Controls whether or not the SD card is used. If not, all output is printed via Serial. Commented out = SD card enabled.
const unsigned long READ_INTERVAL_MINUTES = 15UL;  // Controls how often you want to record a voltage reading. Number must end in UL. Units are in minutes.
const uint8_t NUMBER_OF_SAMPLES_PER_AVERAGE = 3;   // Controls how many times the ADC input is read (sampled), to be averaged and considered one reading.
const uint8_t TIME_BETWEEN_SAMPLES_MS = 50;        // Controls the duration between ADC sampling. Units are in milliseconds.

// Comment out (prefix line with two forward-slashes, like this line) any defines below for unused inputs.
#define USE_INPUT_A0
#define USE_INPUT_A1
#define USE_INPUT_A2
#define USE_INPUT_A3
#define USE_INPUT_A4
#define USE_INPUT_A5

// Modify this array to match the in-use inputs list of defines. This list must be in ascending order.
// e.g., to disable inputs A1 and A5, change the array to {A0, A2, A3, A4}
const uint8_t INPUTS[] = {A0, A1, A2, A3, A4, A5};

// Modify this array to scale the printed result to list the real maximum voltage being measured.
// SCALING and INPUTS must have the same number of elements, and must also match the in-use inputs list of defines.
const float SCALING[] = {5.0, 5.0, 5.0, 5.0, 5.0, 5.0};
/* END: The values above can be edited depending on the application. */

unsigned long previousMillis = 0;
const uint8_t NUMBER_INPUTS_USED = sizeof(INPUTS)/sizeof(INPUTS[0]);
uint8_t AxSampleCount = 0;
uint16_t AxSamplingSums[NUMBER_INPUTS_USED];

const uint8_t STATE_INITIAL = 0;
const uint8_t STATE_READING = 1;
const uint8_t STATE_WRITING = 2;
const uint8_t STATE_WAITING = 3;
uint8_t currentState = STATE_INITIAL;

File outputFile;

/* Wrap Serial println function for convenience logging */
void LOG(const char* msg) {
  Serial.println(msg);
}

/* Wrap Serial println function for convenience logging, disabled when SD card is disabled */
void LOGC(const char* msg) {
  #ifndef DISABLE_SD_CARD
    LOG(msg);
  #endif
}

void LOG_ERROR_AND_FAIL(const char* msg) {
  Serial.println(msg);
  // The "failure" here is to get stuck in an infinite loop, and not do anything else.
  while (1);
}

/* Sanity check that the array length matches the number of defined inputs */
void validateParameters() {
  int8_t numPins = NUMBER_INPUTS_USED;
  #ifdef USE_INPUT_A0
  numPins--;
  #endif
  #ifdef USE_INPUT_A1
  numPins--;
  #endif
  #ifdef USE_INPUT_A2
  numPins--;
  #endif
  #ifdef USE_INPUT_A3
  numPins--;
  #endif
  #ifdef USE_INPUT_A4
  numPins--;
  #endif
  #ifdef USE_INPUT_A5
  numPins--;
  #endif
  if (numPins != 0) {
    LOG_ERROR_AND_FAIL("ERROR: mismatch between #defines and NUMBER_INPUTS_USED variable! Program will not continue.");
  }

  if (NUMBER_INPUTS_USED != sizeof(SCALING)/sizeof(SCALING[0])) {
    LOG_ERROR_AND_FAIL("ERROR: mismatch between INPUTS and SCALING array lengths! Program will not continue.");
  }
}

/* Initializes the SD library and opens the CSV file for writing. */
void initializeSDCard() {
  #ifndef DISABLE_SD_CARD
    LOG("Initializing SD card...");

    if (!SD.begin(4)) {  // 4 is SD_CS_PIN for the UNO
      LOG_ERROR_AND_FAIL("ERORR: SD card initialization failed! Program will not continue.");
    }
    LOG("SD card initialization done");

    outputFile = SD.open("VOLTAGES.CSV", FILE_WRITE);
    if (!outputFile) {
      LOG_ERROR_AND_FAIL("ERROR: Unable to open file for writing! Program will not continue.");
    }
  #else
    LOG("DISABLE_SD_CARD is defined - skipping all SD card activity");
  #endif
}

/* Wrap/combine File and Serial print functions - string input */
void print(const char* p) {
  #ifndef DISABLE_SD_CARD
    outputFile.print(p);
  #endif
  Serial.print(p);
}

/* Wrap/combine File and Serial print functions - float input */
void print(const float f) {
  #ifndef DISABLE_SD_CARD
    outputFile.print(f);
  #endif
  Serial.print(f);
}

/* Wrap/combine File and Serial println functions - string input */
void println(const char* p) {
  #ifndef DISABLE_SD_CARD
    outputFile.println(p);
  #endif
  Serial.println(p);
}

/* Wrap/combine File flush function (no need for Serial function at this time) */
void flush() {
  #ifndef DISABLE_SD_CARD
    outputFile.flush();
  #endif
}

/* Writes the column labels, only for enabled inputs. */
void writeHeaderToOutput() {
  print("T");
  #ifdef USE_INPUT_A0
  print(",V0");
  #endif
  #ifdef USE_INPUT_A1
  print(",V1");
  #endif
  #ifdef USE_INPUT_A2
  print(",V2");
  #endif
  #ifdef USE_INPUT_A3
  print(",V3");
  #endif
  #ifdef USE_INPUT_A4
  print(",V4");
  #endif
  #ifdef USE_INPUT_A5
  print(",V5");
  #endif
  print("\n");
  flush();
}

/* Arduino function called one time to execute the above 3 functions. */
void setup() {
  Serial.begin(9600);

  // Infinite loop to wait for serial bus to be ready (so we don't try to write to it before it's ready).
  while (!Serial) {
    ;
  }

  Serial.print("\n\n");

  validateParameters();
  initializeSDCard();
  writeHeaderToOutput();
}

/* Read the enabled analog inputs and add the results to the sample total. */
void readAnalogInputs() {
  for (uint8_t i = 0; i < NUMBER_INPUTS_USED; i++) {
    AxSamplingSums[i] += analogRead(INPUTS[i]);
  }
  AxSampleCount++;
}

/* Retrieves a given input index's scale factor. */
float getAxScaleFactor(uint16_t idx) {
  return SCALING[idx];
}

/* Calculate the average ADC reading for each enabled input. */
uint16_t getAxSamplesAverage(uint8_t idx) {
  return (uint16_t) AxSamplingSums[idx]/NUMBER_OF_SAMPLES_PER_AVERAGE;
}

/* Scales a 10-bit ADC reading to a referenced voltage. */
float sampleAsVoltage(uint16_t adcValue, float scale) {
  return adcValue * (scale / 1023.0);
}

/* Retrieves a given input index's reading as a scaled voltage. */
float getAxVoltage(uint8_t inputIndex) {
  return sampleAsVoltage(getAxSamplesAverage(inputIndex), getAxScaleFactor(inputIndex));
}

/* Record the output data to a line. */
void writeAveragesToOutput() {
  const unsigned long currentTime = millis();
  float voltage;
  print(currentTime);
  print(",");
  for (uint8_t input = 0; input < NUMBER_INPUTS_USED; input++) {
    voltage = getAxVoltage(input);
    print(voltage);

    // For the last line entry, don't print a comma.
    if (input == NUMBER_INPUTS_USED - 1) {
      break;
    }
    print(",");
  }
  print("\n");
  flush();
}

/* Clear the sample totals for all enabled inputs. Done after the average is calculated to prepare for the next sampling. */
void resetSampleData() {
  AxSampleCount = 0;
  for (uint8_t i = 0; i < NUMBER_INPUTS_USED; i++) {
    AxSamplingSums[i] = 0;
  }
}

/* Determines if it's time for another sample by calculating the elapsed time between now and the previous reading.
   The calculation/comparison is short-circuited by the previous reading time to ensure we don't have to wait
   TIME_BETWEEN_SAMPLES_MS before the first reading can occur.
*/
bool isTooSoonForReading(unsigned long currentMillis) {
  return previousMillis && (currentMillis - previousMillis < TIME_BETWEEN_SAMPLES_MS);
}

/* Determines if enough time has passed between readings. */
bool hasWaitingPeriodElapsed(unsigned long currentMillis) {
  return currentMillis - previousMillis > READ_INTERVAL_MINUTES * 60UL * 1000UL;
}

/* Determines if we've sampled the ADC enough times to make a calculation for the current reading. */
bool moreSamplesNeeded() {
  return AxSampleCount < NUMBER_OF_SAMPLES_PER_AVERAGE;
}

/* Arduino function called infinitely. This code implements a pseudo-scheduler by way of a state-machine because it's easier to read and follow.
   State sequence is INITIALIZING -> READING -> WRITING -> WAITING -> READING -> ...
*/
void loop() {
  unsigned long currentMillis = millis();
  switch (currentState) {
    case STATE_READING:
      if (isTooSoonForReading(currentMillis)) {
        break;
      }

      previousMillis = currentMillis;
      readAnalogInputs();
      if (moreSamplesNeeded()) {
        break;
      }

      LOGC("Inputs read");
      currentState = STATE_WRITING;
      break;
    case STATE_WRITING:
      writeAveragesToOutput();
      resetSampleData();
      LOGC("Data written");
      currentState = STATE_WAITING;
      break;
    case STATE_WAITING:
      if (hasWaitingPeriodElapsed(currentMillis)) {
        currentState = STATE_READING;
      }
      break;
    case STATE_INITIAL:
    default:  // deliberate fall-through
      currentState = STATE_READING;
      break;
  }
}
