/*
 * Inferencia Edge Impulse con fusión de sensores (IMU de 9 ejes)
 * Arduino Nano 33 BLE Sense Lite (LSM9DS1)
 *
 * Basado en el ejemplo nano_ble33_sense_fusion de la librería arduino-ble_inferencing.
 * El modelo espera 9 ejes en este orden: accX, accY, accZ, gyrX, gyrY, gyrZ, magX, magY, magZ.
 * Clases: apagado, velocidadmaxima, velocidadmaximaobstruida, velocidadminima.
 */

#include <Arduino.h>
#include <arduino-ble_inferencing.h>
#include <Arduino_LSM9DS1.h>

#if !defined(EI_CLASSIFIER_SENSOR) || (EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_FUSION)
#error "El modelo no es de fusión de sensores"
#endif

// El modelo debe tener exactamente los 9 ejes del IMU
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 9, "El modelo espera 9 ejes (acc + gyr + mag)");

#define CONVERT_G_TO_MS2 9.80665f  // el modelo se entrenó con aceleración en m/s^2
#define MAX_ACCEPTED_RANGE 2.0f    // límite de aceleración en g (igual que el ejemplo)

static bool debug_nn = false;  // true para imprimir las features del DSP

// Última lectura de cada eje: acc[0..2], gyr[3..5], mag[6..8].
// Si el sensor no tiene dato nuevo se repite la lectura anterior.
static float imu[9] = { 0 };

static float clip_accel(float g) {
    if (g > MAX_ACCEPTED_RANGE) return MAX_ACCEPTED_RANGE;
    if (g < -MAX_ACCEPTED_RANGE) return -MAX_ACCEPTED_RANGE;
    return g;
}

static void poll_imu() {
    if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(imu[0], imu[1], imu[2]);
        for (int i = 0; i < 3; i++) {
            imu[i] = clip_accel(imu[i]) * CONVERT_G_TO_MS2;
        }
    }
    if (IMU.gyroscopeAvailable()) {
        IMU.readGyroscope(imu[3], imu[4], imu[5]);
    }
    if (IMU.magneticFieldAvailable()) {
        IMU.readMagneticField(imu[6], imu[7], imu[8]);
    }
}

void setup() {
    Serial.begin(115200);
    // Espera la conexión USB, pero no se queda colgado si no hay monitor serie
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) {
    }

    ei_printf("Edge Impulse Sensor Fusion Inference (%s)\r\n", EI_CLASSIFIER_PROJECT_NAME);

    if (!IMU.begin()) {
        ei_printf("ERR: no se pudo inicializar el IMU LSM9DS1\r\n");
        while (true) {
            delay(1000);
        }
    }

    ei_printf("Ejes requeridos: %s\r\n", EI_CLASSIFIER_FUSION_AXES_STRING);
    ei_printf("Ventana: %d ms, %d Hz\r\n",
              (int)(EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_INTERVAL_MS),
              (int)EI_CLASSIFIER_FREQUENCY);
}

void loop() {
    ei_printf("\r\nStarting inferencing in 2 seconds...\r\n");
    delay(2000);

    ei_printf("Sampling...\r\n");

    // Buffer con la ventana completa de muestras (RAW_SAMPLE_COUNT x 9 ejes)
    static float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
        int64_t next_tick = (int64_t)micros() + ((int64_t)EI_CLASSIFIER_INTERVAL_MS * 1000);

        poll_imu();
        for (int i = 0; i < EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME; i++) {
            buffer[ix + i] = imu[i];
        }

        int64_t wait_time = next_tick - (int64_t)micros();
        if (wait_time > 0) {
            delayMicroseconds(wait_time);
        }
    }

    // Convierte el buffer crudo en una señal que el clasificador pueda leer
    signal_t signal;
    int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0) {
        ei_printf("ERR: signal_from_buffer (%d)\r\n", err);
        return;
    }

    ei_impulse_result_t result = { 0 };
    err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: run_classifier (%d)\r\n", err);
        return;
    }

    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.):\r\n",
              result.timing.dsp, result.timing.classification, result.timing.anomaly);

    size_t best = 0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("  %s: %.5f\r\n", result.classification[ix].label, result.classification[ix].value);
        if (result.classification[ix].value > result.classification[best].value) {
            best = ix;
        }
    }
    ei_printf("=> %s (%.1f%%)\r\n", result.classification[best].label, result.classification[best].value * 100.0f);

    // Línea JSON (formato TagoIO) para que el puente serie->MQTT la publique.
    // Empieza con '[' para distinguirla del resto de los mensajes.
    static uint32_t inference_id = 0;
    inference_id++;
    ei_printf("[{\"variable\":\"clase\",\"value\":\"%s\",\"group\":\"inf%lu\",\"metadata\":{\"confianza\":%.4f}}",
              result.classification[best].label, (unsigned long)inference_id, result.classification[best].value);
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf(",{\"variable\":\"prob_%s\",\"value\":%.5f,\"group\":\"inf%lu\"}",
                  result.classification[ix].label, result.classification[ix].value, (unsigned long)inference_id);
    }
    ei_printf("]\r\n");

#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("  anomaly score: %.3f\r\n", result.anomaly);
#endif
}
